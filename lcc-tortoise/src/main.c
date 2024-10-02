/*
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/sys/reboot.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"
#include "dcc-decode.h"
#include "tortoise-cdi.h"
#include "power-handler.h"
#include "firmware_upgrade.h"

#include "lcc.h"
#include "lcc-common.h"
#include "lcc-event.h"
#include "lcc-memory.h"

#include "dcc-decoder.h"
#include "dcc-packet-parser.h"

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* 251 = node name/description */
#define ADDRESS_SPACE_251_OFFSET 0
/* 253 = basic config space */
#define ADDRESS_SPACE_253_OFFSET 128
#define GLOBAL_CONFIG_OFFSET 2048

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static struct can_frame zephyr_can_frame_tx;
static struct lcc_can_frame lcc_rx;

static struct gpio_callback blue_button_cb_data;
static struct gpio_callback gold_button_cb_data;

struct k_work state_change_work;
enum can_state current_state;
struct can_bus_err_cnt current_err_cnt;

#define STATE_POLL_THREAD_STACK_SIZE 512
#define STATE_POLL_THREAD_PRIORITY 2

K_THREAD_STACK_DEFINE(poll_state_stack, STATE_POLL_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(dcc_signal_stack, 512);

struct k_thread poll_state_thread_data;
struct k_thread dcc_thread_data;
CAN_MSGQ_DEFINE(rx_msgq, 55);

char *state_to_str(enum can_state state)
{
	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		return "error-active";
	case CAN_STATE_ERROR_WARNING:
		return "error-warning";
	case CAN_STATE_ERROR_PASSIVE:
		return "error-passive";
	case CAN_STATE_BUS_OFF:
		return "bus-off";
	case CAN_STATE_STOPPED:
		return "stopped";
	default:
		return "unknown";
	}
}

void poll_state_thread(void *unused1, void *unused2, void *unused3)
{
	struct can_bus_err_cnt err_cnt = {0, 0};
	struct can_bus_err_cnt err_cnt_prev = {0, 0};
	enum can_state state_prev = CAN_STATE_ERROR_ACTIVE;
	enum can_state state;
	int err;

	while (1) {
		err = can_get_state(can_dev, &state, &err_cnt);
		if (err != 0) {
			printf("Failed to get CAN controller state: %d", err);
			k_sleep(K_MSEC(100));
			continue;
		}

		if (err_cnt.tx_err_cnt != err_cnt_prev.tx_err_cnt ||
		    err_cnt.rx_err_cnt != err_cnt_prev.rx_err_cnt ||
		    state_prev != state) {

			err_cnt_prev.tx_err_cnt = err_cnt.tx_err_cnt;
			err_cnt_prev.rx_err_cnt = err_cnt.rx_err_cnt;
			state_prev = state;
			printf("state: %s\n"
			       "rx error count: %d\n"
			       "tx error count: %d\n",
			       state_to_str(state),
			       err_cnt.rx_err_cnt, err_cnt.tx_err_cnt);
		} else {
			k_sleep(K_MSEC(100));
		}
	}
}

void state_change_work_handler(struct k_work *work)
{
	printf("State Change ISR\nstate: %s\n"
	       "rx error count: %d\n"
	       "tx error count: %d\n",
		state_to_str(current_state),
		current_err_cnt.rx_err_cnt, current_err_cnt.tx_err_cnt);
}

void state_change_callback(const struct device *dev, enum can_state state,
			   struct can_bus_err_cnt err_cnt, void *user_data)
{
	struct k_work *work = (struct k_work *)user_data;

	ARG_UNUSED(dev);

	current_state = state;
	current_err_cnt = err_cnt;
	k_work_submit(work);
}

static int lcc_write_cb(struct lcc_context*, struct lcc_can_frame* lcc_frame){
	memset(&zephyr_can_frame_tx, 0, sizeof(zephyr_can_frame_tx));
	zephyr_can_frame_tx.id = lcc_frame->can_id;
	zephyr_can_frame_tx.dlc = lcc_frame->can_len;
	zephyr_can_frame_tx.flags = CAN_FRAME_IDE;
	memcpy(zephyr_can_frame_tx.data, lcc_frame->data, 8);

	printf("Send frame 0x%08X\n", lcc_frame->can_id);

	int stat = can_send(can_dev, &zephyr_can_frame_tx, K_FOREVER, NULL, NULL );
	if( stat == 0 ){
		return LCC_OK;
	}

	return LCC_ERROR_TX;
}

static void incoming_event(struct lcc_context* ctx, uint64_t event_id){
	for(int x = 0; x < 8; x++){
		tortoise_incoming_event(&lcc_tortoise_state.tortoises[x], event_id);
	}
}

static void init_rx_queue(){
	const struct can_filter filter = {
			.flags = CAN_FILTER_IDE,
			.id = 0x0,
			.mask = 0
	};
	struct can_frame frame;
	int filter_id;

	filter_id = can_add_rx_filter_msgq(can_dev, &rx_msgq, &filter);
	printf("Filter ID: %d\n", filter_id);
}

/**
 * Load the tortoise settings.
 *
 * @return 0 if settings were loaded, 1 if they were not, 2 if settings are blank
 */
static int load_tortoise_settings(){
	uint32_t offset = ADDRESS_SPACE_253_OFFSET;
	int ret = 0;

	const struct flash_area* storage_area = NULL;
	int id = FIXED_PARTITION_ID(storage_partition);

	if(flash_area_open(id, &storage_area) < 0){
		return 1;
	}

	if(flash_area_read(storage_area, offset, &lcc_tortoise_state.tortoise_config, sizeof(lcc_tortoise_state.tortoise_config)) < 0){
		return 1;
	}

	// Check to see if the first 8 bytes are 0xFF.  If so, we assume the data is uninitialized
	const uint8_t mem[8] = {
			0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF
	};
	if(memcmp(lcc_tortoise_state.tortoise_config, mem, 8) == 0){
		ret = 2;
		goto out;
	}

	for(int x = 0; x < 8; x++){
		tortoise_init_startup_position(&lcc_tortoise_state.tortoises[x]);
	}

out:
	flash_area_close(storage_area);
	return ret;
}

void mem_address_space_information_query(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space){
	if(address_space == 251){
		lcc_memory_respond_information_query(ctx, alias, 1, address_space, 64 + 64, 0, 0);
	}else if(address_space == 253){
		// basic config space
		// 32 bytes * 8 tortoises
		uint32_t upper_address = sizeof(struct tortoise_config) * 8;
		lcc_memory_respond_information_query(ctx, alias, 1, address_space, upper_address, 0, 0);
	}else{
		// This memory space does not exist: return an error
		lcc_memory_respond_information_query(ctx, alias, 0, address_space, 0, 0, 0);
	}
}

void mem_address_space_read(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space, uint32_t starting_address, uint8_t read_count){
	 if(address_space == 251){
	    // This space is what we use for node name/description
	    uint8_t buffer[64];
//	    int rc = eeprom_read(lcc_tortoise_state.fram, ADDRESS_SPACE_251_OFFSET + starting_address, &buffer, read_count);
	    int rc = -1;
	    if(rc < 0){
	    	lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
	    	return;
	    }

	    // For any blank data, we will read 0xFF
	    // In this example, we know that we have strings, so replace all 0xFF with 0x00
	    for(int x = 0; x < sizeof(buffer); x++){
	      if(buffer[x] == 0xFF){
	        buffer[x] = 0x00;
	      }
	    }

	    lcc_memory_respond_read_reply_ok(ctx, alias, address_space, starting_address, buffer, read_count);
	  }else if(address_space == 253){
		// Basic config space
		if(starting_address + read_count > (sizeof(lcc_tortoise_state.tortoise_config))){
		  // trying to read too much memory
		  lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
		  return;
		}

	    uint8_t* config_as_u8 = (uint8_t*)lcc_tortoise_state.tortoise_config;

	    lcc_memory_respond_read_reply_ok(ctx, alias, address_space, starting_address, config_as_u8 + starting_address, read_count);
	  }
}

int save_configs_to_flash(){
	uint32_t offset = ADDRESS_SPACE_253_OFFSET;
	int ret = 0;

	const struct flash_area* storage_area = NULL;
	int id = FIXED_PARTITION_ID(storage_partition);

	if(flash_area_open(id, &storage_area) < 0){
		return 1;
	}

	ret = flash_area_erase(storage_area, 0, sizeof(lcc_tortoise_state.tortoise_config));
	if(ret){
		goto out;
	}

	if(flash_area_write(storage_area, offset, &lcc_tortoise_state.tortoise_config, sizeof(lcc_tortoise_state.tortoise_config)) < 0){
		ret = 1;
	}

out:
	flash_area_close(storage_area);
	if(ret){
		printf("Unable to save configs to flash\n");
	}
	return ret;
}

void mem_address_space_write(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space, uint32_t starting_address, void* data, int data_len){
	  if(address_space == 251){
//	    eeprom_write(lcc_tortoise_state.fram, ADDRESS_SPACE_251_OFFSET + starting_address, data, data_len);

	    lcc_memory_respond_write_reply_ok(ctx, alias, address_space, starting_address);
	  }else if(address_space == 253){
		  if((starting_address + data_len) > sizeof(lcc_tortoise_state.tortoise_config)){
			    lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
			    return;
		  }
		  uint8_t* buffer_u8 = (uint8_t*)lcc_tortoise_state.tortoise_config;

		  memcpy(buffer_u8 + starting_address, data, data_len);

	    // Write out to non-volatile memory
	    save_configs_to_flash();

	    lcc_memory_respond_write_reply_ok(ctx, alias, address_space, starting_address);
	  }else{
	    lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
	    return;
	  }
}

static void reboot(struct lcc_memory_context* ctx){
	sys_reboot(SYS_REBOOT_COLD);
}

static void factory_reset(struct lcc_memory_context* ctx){
	memset(&lcc_tortoise_state.tortoise_config, 0, sizeof(lcc_tortoise_state.tortoise_config));

	for(int x = 0; x < 8; x++){
		lcc_tortoise_state.tortoise_config[x].BE_accessory_number = __builtin_bswap16(x + 1);
		lcc_tortoise_state.tortoise_config[x].startup_control = STARTUP_NORMAL;
	}

	save_configs_to_flash();
}

static enum lcc_consumer_state query_consumer_state(struct lcc_context* ctx, uint64_t event_id){
	for(int x = 0; x < 8; x++){
		uint64_t* events = tortoise_events_consumed(&lcc_tortoise_state.tortoises[x]);

		if(event_id == events[0]){
			// reverse/off
			if(lcc_tortoise_state.tortoises[x].current_position == POSITION_REVERSE){
				return LCC_CONSUMER_VALID;
			}
			return LCC_CONSUMER_INVALID;
		}else if(event_id == events[1]){
			// normal/on
			if(lcc_tortoise_state.tortoises[x].current_position == POSITION_NORMAL){
				return LCC_CONSUMER_VALID;
			}
			return LCC_CONSUMER_INVALID;
		}
	}

	return LCC_CONSUMER_UNKNOWN;
}

static void speed_dir_cb(struct dcc_decoder* decoder, enum dcc_decoder_direction dir, uint8_t speed){
	printf("dir: %d speed: %d\n", dir, speed);
}

static void incoming_dcc(struct dcc_decoder* decoder, const uint8_t* packet_bytes, int len){
	static int count = 0;

	count++;
	if(count % 50 == 0){
		gpio_pin_toggle_dt(&lcc_tortoise_state.blue_led);
	}
}

int main(void)
{
	int ret;
	bool led_state = true;
	k_tid_t rx_tid, get_state_tid;
	k_tid_t dcc_thread;

	// Sleep for a bit before we start to allow power to become stable.
	// This is probably not needed, but it shouldn't hurt.
	// This will also give other devices on the network a chance to come up.
	k_sleep(K_MSEC(250));

	for(int x = 0; x < 15; x++){
	printf("farts\n");
	k_sleep(K_MSEC(100));
	}

	printf("sys_clock_hw_cycles_per_sec() / USEC_PER_SEC = %lu / %lu\n", sys_clock_hw_cycles_per_sec() , USEC_PER_SEC);
	printf("cycles per usec = %lu\n", sys_clock_hw_cycles_per_sec() /USEC_PER_SEC);

	_Static_assert(sizeof(struct tortoise_config) == 32);

	if(lcc_tortoise_state_init() < 0){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return 0;
	}

	// Load the tortoise settings, and set the outputs to the correct valu depending on their startup settings
	int tortoise_load_stat = load_tortoise_settings();
	if(tortoise_load_stat == 1){
		printf("Error: Unable to load settings!\n");
		return 1;
	}else if(tortoise_load_stat == 2){
		printf("Chip is blank, initializing to default values...\n");
		factory_reset(NULL);
	}
	for(int x = 0; x < 8; x++){
		tortoise_set_position(&lcc_tortoise_state.tortoises[x], lcc_tortoise_state.tortoises[x].current_position);
	}

	if (!device_is_ready(can_dev)) {
		printf("CAN: Device %s not ready.\n", can_dev->name);
		return 0;
	}

	ret = can_start(can_dev);
	if(ret != 0){
		printf("Can't start CAN\n");
		return 0;
	}

	k_work_init(&state_change_work, state_change_work_handler);

	get_state_tid = k_thread_create(&poll_state_thread_data,
					poll_state_stack,
					K_THREAD_STACK_SIZEOF(poll_state_stack),
					poll_state_thread, NULL, NULL, NULL,
					STATE_POLL_THREAD_PRIORITY, 0,
					K_NO_WAIT);
	if (!get_state_tid) {
		printf("ERROR spawning poll_state_thread\n");
	}

	can_set_state_change_callback(can_dev, state_change_callback, &state_change_work);

	init_rx_queue();

//	check_eeprom();

	lcc_tortoise_state.dcc_decoder = dcc_decoder_new(DCC_DECODER_IRQ_RISING_OR_FALLING);
	if(lcc_tortoise_state.dcc_decoder == NULL){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return 0;
	}

//	dcc_thread = k_thread_create(&dcc_thread_data,
//			dcc_signal_stack,
//			K_THREAD_STACK_SIZEOF(dcc_signal_stack),
//			dcc_decoder_thread, NULL, NULL, NULL,
//			0, 0,
//			K_NO_WAIT);
//	if (!dcc_thread) {
//		printf("ERROR spawning dcc thread\n");
//	}

	lcc_tortoise_state.packet_parser = dcc_packet_parser_new();
	if(lcc_tortoise_state.packet_parser == NULL){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return 0;
	}

	lcc_tortoise_state.lcc_context = lcc_context_new();
	if(lcc_tortoise_state.lcc_context == NULL){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return 0;
	}

	dcc_decoder_set_packet_callback(lcc_tortoise_state.dcc_decoder, incoming_dcc);
	dcc_decoder_set_packet_parser(lcc_tortoise_state.dcc_decoder, lcc_tortoise_state.packet_parser);

	struct lcc_context* ctx = lcc_tortoise_state.lcc_context;
	lcc_context_set_unique_identifer( ctx,
			0x020202000012ll );
	lcc_context_set_simple_node_information(ctx,
			"Snowball Creek",
			"Tortoise-8",
			"P3",
			"0.3");

	lcc_context_set_write_function( ctx, lcc_write_cb, NULL );
	struct lcc_event_context* evt_ctx = lcc_event_new(ctx);

	lcc_event_set_incoming_event_function(evt_ctx, incoming_event);
	lcc_event_add_event_consumed_query_fn(evt_ctx, query_consumer_state);

	for(int x = 0; x < 8; x++){
		uint64_t* events_consumed = tortoise_events_consumed(&lcc_tortoise_state.tortoises[x]);
		lcc_event_add_event_consumed(evt_ctx, events_consumed[0]);
		lcc_event_add_event_consumed(evt_ctx, events_consumed[1]);
	}

	lcc_datagram_context_new(ctx);
	struct lcc_memory_context* mem_ctx = lcc_memory_new(ctx);
	lcc_memory_set_cdi(mem_ctx, cdi, sizeof(cdi), 0);
	lcc_memory_set_memory_functions(mem_ctx,
	    mem_address_space_information_query,
	    mem_address_space_read,
	    mem_address_space_write);
	lcc_memory_set_reboot_function(mem_ctx, reboot);
	lcc_memory_set_factory_reset_function(mem_ctx, factory_reset);

	firmware_upgrade_init(ctx);

	int stat = lcc_context_generate_alias( ctx );
	if(stat < 0){
		printf("error: can't gen alias: %d\n", stat);
		return 0;
	}
	uint64_t claim_alias_time = k_uptime_get() + 250;
	uint64_t blinky_time = claim_alias_time;
	struct can_frame rx_frame;

	dcc_packet_parser_set_short_address(lcc_tortoise_state.packet_parser , 55);
	dcc_packet_parser_set_speed_dir_cb(lcc_tortoise_state.packet_parser , speed_dir_cb);
//	while(1){
//		k_sleep(K_MSEC(1000));
//	}

	while (1) {
		if(k_uptime_get() >= blinky_time){
			ret = gpio_pin_toggle_dt(&lcc_tortoise_state.green_led);
			if (ret < 0) {
				return 0;
			}

			led_state = !led_state;
			blinky_time = k_uptime_get() + 250;
		}

		if(k_uptime_get() >= claim_alias_time &&
		    lcc_context_current_state(ctx) == LCC_STATE_INHIBITED){
		    int stat = lcc_context_claim_alias(ctx);
		    if(stat != LCC_OK){
		      // If we were unable to claim our alias, we need to generate a new one and start over
		      lcc_context_generate_alias(ctx);
		      claim_alias_time = k_uptime_get() + 220;
		    }else{
		      printf("Claimed alias %X\n", lcc_context_alias(ctx) );

				// initialize our low power handling.
				// We do this right after we have an alias, since logically nothing will have changed
				// until we are actually able to process commands from some other device on the network.
				powerhandle_init();
		    }
		  }

		// Receive a CAN frame if there is any
		if(k_msgq_get(&rx_msgq, &rx_frame, K_NO_WAIT) == 0){
			memset(&lcc_rx, 0, sizeof(lcc_rx));
			lcc_rx.can_id = rx_frame.id;
			lcc_rx.can_len = rx_frame.dlc;
			memcpy(lcc_rx.data, rx_frame.data, 8);

			printf("Incoming frame: %x\n", lcc_rx.can_id);
			lcc_context_incoming_frame(ctx, &lcc_rx);
		}

		k_sleep(K_MSEC(1));
		dcc_decoder_pump_packet(lcc_tortoise_state.dcc_decoder);
	}
	return 0;
}
