/*
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/console/console.h>
#include <zephyr/dfu/mcuboot.h>
#include "dcc-decode-stm32.h"

#include "lcc-tortoise-state.h"
#include "tortoise.h"
#include "tortoise-cdi.h"
#include "power-handler.h"
#include "firmware_upgrade.h"
#include "switch-tracker.h"

#include "lcc.h"
#include "lcc-common.h"
#include "lcc-event.h"
#include "lcc-memory.h"

#include "dcc-decoder.h"
#include "dcc-packet-parser.h"

#define VERSION_STR "0.4"

/*
 * Address Space / storage information
 * 253 = basic config space.  This is stored in RAM during run-time.
 *   If it changes, it is written to the config_partition location
 * 251 = node name/description.  This is stored in the global_partition
 * 250 = DCC translation configuration.  Stored in the global_partition, but handled separately over LCC
 * 249 = firmware version header information
 *
 * LCC ID is stored in the lcc_partition.  This partition is effectively readonly,
 * it should never be overwritten as it is possible that writing to it might occur
 * at a bad time causing the LCC ID to be lost.
 *
 * The base event ID(when the system is reset) is stored in the global_partition.
 * This is for the LCC specification that when a node is reset to factory defaults,
 * the eventIDs need to not repeat.
 */

struct dcc_address_translation_config{
	int do_dcc_translation;
};

struct global_config_data {
	char node_name[64];
	char node_description[64];
	uint64_t base_event_id;
	struct dcc_address_translation_config dcc_translation;
};

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static struct lcc_can_frame lcc_rx;
static struct global_config_data global_config;

static struct gpio_callback blue_button_cb_data;
static struct gpio_callback gold_button_cb_data;

K_THREAD_STACK_DEFINE(tx_stack, 512);
struct k_thread tx_thread_data;
k_tid_t tx_thread_tid;
CAN_MSGQ_DEFINE(rx_msgq, 55);
CAN_MSGQ_DEFINE(tx_msgq, 12);

static void blink_led_green(){
	while(1){
		if(lcc_tortoise_state.button_control != BUTTON_CONTROL_NORMAL){
			// LED should be on, wink off once every three seconds
			gpio_pin_set_dt(&lcc_tortoise_state.green_led, 1);
			k_sleep(K_MSEC(3000));
			gpio_pin_set_dt(&lcc_tortoise_state.green_led, 0);
			k_sleep(K_MSEC(100));
		}else{
			uint64_t diff = k_cycle_get_32() - lcc_tortoise_state.last_rx_dcc_msg;

			if(k_cyc_to_ms_ceil32(diff) < 500){
				// Receiving DCC signal, double blink
				gpio_pin_set_dt(&lcc_tortoise_state.green_led, 1);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&lcc_tortoise_state.green_led, 0);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&lcc_tortoise_state.green_led, 1);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&lcc_tortoise_state.green_led, 0);
				k_sleep(K_MSEC(1500));
			}else{
				// No DCC signal, single blink
				gpio_pin_set_dt(&lcc_tortoise_state.green_led, 1);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&lcc_tortoise_state.green_led, 0);
				k_sleep(K_MSEC(1500));
			}
		}
	}
}

static void blink_led_blue(){
	while(1){

		if(lcc_tortoise_state.button_control != BUTTON_CONTROL_NORMAL){
			// LED should blink with the mode, waiting two seconds between blinks
			for(int x = 0; x < lcc_tortoise_state.button_control; x++){
				gpio_pin_set_dt(&lcc_tortoise_state.blue_led, 1);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&lcc_tortoise_state.blue_led, 0);
				k_sleep(K_MSEC(100));
			}
			k_sleep(K_SECONDS(2));
		}else{
			uint64_t diff = k_cycle_get_32() - lcc_tortoise_state.last_rx_can_msg;

			if(k_cyc_to_ms_ceil32(diff) < 25){
				gpio_pin_set_dt(&lcc_tortoise_state.blue_led, 1);
				k_sleep(K_MSEC(50));
				gpio_pin_set_dt(&lcc_tortoise_state.blue_led, 0);
			}
			k_sleep(K_MSEC(10));
		}
	}
}

static void blink_led_gold(){
	while(1){
		uint64_t diff = k_cycle_get_32() - lcc_tortoise_state.last_tx_can_msg;

		if(k_cyc_to_ms_ceil32(diff) < 25){
			gpio_pin_set_dt(&lcc_tortoise_state.gold_led, 1);
			k_sleep(K_MSEC(50));
			gpio_pin_set_dt(&lcc_tortoise_state.gold_led, 0);
		}
		k_sleep(K_MSEC(10));
	}
}

K_THREAD_DEFINE(green_blink, 512, blink_led_green, NULL, NULL, NULL,
		7, 0, 0);
K_THREAD_DEFINE(blue_blink, 512, blink_led_blue, NULL, NULL, NULL,
		7, 0, 0);
K_THREAD_DEFINE(gold_blink, 512, blink_led_gold, NULL, NULL, NULL,
		7, 0, 0);

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

static void can_frame_send_thread(void *unused1, void *unused2, void *unused3)
{
	static struct can_frame tx_frame;
	struct can_bus_err_cnt err_cnt = {0, 0};
	enum can_state state;

	while(1){
		if(k_msgq_get(&tx_msgq, &tx_frame, K_FOREVER) == 0){
			int err = can_get_state(can_dev, &state, &err_cnt);
			if(err != 0){
				printf("Can't get CAN state\n");
				continue;
			}

			if(state == CAN_STATE_BUS_OFF){
				printf("CAN bus off, dropping packet\n");
				continue;
			}

			int stat = can_send(can_dev, &tx_frame, K_FOREVER, NULL, NULL );
			if(stat < 0){
				printf("Unable to send: %d\n", stat);
			}

			lcc_tortoise_state.last_tx_can_msg = k_cycle_get_32();
		}
	}
}

static int lcc_write_cb(struct lcc_context*, struct lcc_can_frame* lcc_frame){
	static struct can_frame zephyr_can_frame_tx;
	memset(&zephyr_can_frame_tx, 0, sizeof(zephyr_can_frame_tx));
	zephyr_can_frame_tx.id = lcc_frame->can_id;
	zephyr_can_frame_tx.dlc = lcc_frame->can_len;
	zephyr_can_frame_tx.flags = CAN_FRAME_IDE;
	memcpy(zephyr_can_frame_tx.data, lcc_frame->data, 8);

	if(k_msgq_put(&tx_msgq, &zephyr_can_frame_tx, K_NO_WAIT) < 0){
		return LCC_ERROR_TX;
	}

	return LCC_OK;
}

static void incoming_event(struct lcc_context* ctx, uint64_t event_id){
	for(int x = 0; x < 8; x++){
		int changed = tortoise_incoming_event(&lcc_tortoise_state.tortoises[x], event_id);
		if(changed){
			set_tortoise_posistions_dirty();
		}
	}
}

static void init_can_txrx(){
	const struct can_filter filter = {
			.flags = CAN_FILTER_IDE,
			.id = 0x0,
			.mask = 0
	};
	int filter_id;

	filter_id = can_add_rx_filter_msgq(can_dev, &rx_msgq, &filter);
	printf("Filter ID: %d\n", filter_id);

	tx_thread_tid = k_thread_create(&tx_thread_data,
				tx_stack,
				K_THREAD_STACK_SIZEOF(tx_stack),
				can_frame_send_thread, NULL, NULL, NULL,
				0, 0,
				K_NO_WAIT);
	if (!tx_thread_tid) {
		printf("ERROR spawning tx thread\n");
	}
}

/**
 * Load the tortoise settings.
 *
 * @return 0 if settings were loaded, 1 if they were not, 2 if settings are blank
 */
static int load_tortoise_settings(){
	int ret = 0;
	uint8_t tortoise_positions[8 * 4];

	const struct flash_area* storage_area = NULL;
	int id = FIXED_PARTITION_ID(config_partition);
	const struct flash_area* location_area = NULL;
	int id2 = FIXED_PARTITION_ID(location_partition);

	if(flash_area_open(id, &storage_area) < 0){
		return 1;
	}

	if(flash_area_open(id2, &location_area) < 0){
		return 1;
	}

	if(flash_area_read(storage_area, 0, &lcc_tortoise_state.tortoise_config, sizeof(lcc_tortoise_state.tortoise_config)) < 0){
		return 1;
	}

	if(flash_area_read(location_area, 0, tortoise_positions, sizeof(tortoise_positions)) < 0){
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
		if(lcc_tortoise_state.tortoise_config[x].startup_control == STARTUP_LAST_POSITION){
			// Figure out the last position
			int count_agree_normal = 0;
			int count_agree_reverse = 0;

			for(int y = 0; y < 4; y++){
				int offset_val = tortoise_positions[x + (8 * y)];
				if(offset_val == 0){
					count_agree_normal++;
				}else if(offset_val == 1){
					count_agree_reverse++;
				}
			}

			printf("Output %d last known pos ", x);
			if(count_agree_normal > count_agree_reverse){
				lcc_tortoise_state.tortoise_config[x].last_known_pos = POSITION_NORMAL;
				printf("normal\n");
			}else if(count_agree_reverse > count_agree_normal){
				lcc_tortoise_state.tortoise_config[x].last_known_pos = POSITION_REVERSE;
				printf("reverse\n");
			}else{
				printf("Unknown\n");
			}
		}

		tortoise_init_startup_position(&lcc_tortoise_state.tortoises[x]);
	}

out:
	flash_area_close(storage_area);
	flash_area_close(location_area);
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
	}else if(address_space == 250){
		// Global config
		lcc_memory_respond_information_query(ctx, alias, 1, address_space, 1, 0, 0);
	}else if(address_space = 249){
		// Firmware versions
		lcc_memory_respond_information_query(ctx, alias, 1, address_space, 16, 0, 0);
	}else{
		// This memory space does not exist: return an error
		lcc_memory_respond_information_query(ctx, alias, 0, address_space, 0, 0, 0);
	}
}

void mem_address_space_read(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space, uint32_t starting_address, uint8_t read_count){
	if(address_space == 251){
		// This space is what we use for node name/description
		uint8_t* buffer = global_config.node_name + starting_address;

		if(starting_address + read_count > 128){
			// trying to read too much memory
			lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
			return;
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
	  }else if(address_space == 250){
		  // Global config
		  uint8_t* buffer = &global_config.dcc_translation + starting_address;

		  if(starting_address + read_count > 1){
				// trying to read too much memory
				lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
				return;
			}

			lcc_memory_respond_read_reply_ok(ctx, alias, address_space, starting_address, buffer, read_count);
	  }else if(address_space == 249){
		  struct mcuboot_img_header versions[2];
		  struct mcuboot_img_sem_ver semver[2] = {0};

		  if(boot_read_bank_header(FIXED_PARTITION_ID(slot0_partition), &versions[0], sizeof(struct mcuboot_img_header)) == 0){
			  semver[0] = versions[0].h.v1.sem_ver;
		  }
		  if(boot_read_bank_header(FIXED_PARTITION_ID(slot1_partition), &versions[1], sizeof(struct mcuboot_img_header)) == 0){
			  semver[1] = versions[1].h.v1.sem_ver;
		  }

		  if(starting_address + read_count > sizeof(semver)){
				// trying to read too much memory
				lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
				return;
			}

		  uint8_t* buffer = ((uint8_t*)&semver) + starting_address;
			lcc_memory_respond_read_reply_ok(ctx, alias, address_space, starting_address, buffer, read_count);
	  }
}

int save_configs_to_flash(){
	int ret = 0;

	const struct flash_area* storage_area = NULL;
	int id = FIXED_PARTITION_ID(config_partition);

	if(flash_area_open(id, &storage_area) < 0){
		return 1;
	}

	ret = flash_area_erase(storage_area, 0, sizeof(lcc_tortoise_state.tortoise_config));
	if(ret){
		goto out;
	}

	if(flash_area_write(storage_area, 0, &lcc_tortoise_state.tortoise_config, sizeof(lcc_tortoise_state.tortoise_config)) < 0){
		ret = 1;
	}

out:
	flash_area_close(storage_area);
	if(ret){
		printf("Unable to save configs to flash\n");
	}

	powerhandle_check_if_save_required();
	return ret;
}

static int save_global_config(){
	const struct flash_area* storage_area = NULL;
	int id = FIXED_PARTITION_ID(global_partition);
	int ret = 0;

	if(flash_area_open(id, &storage_area) < 0){
		return 1;
	}

	ret = flash_area_erase(storage_area, 0, sizeof(global_config));
	if(ret){
		goto out;
	}

	if(flash_area_write(storage_area, 0, &global_config, sizeof(global_config)) < 0){
		ret = 1;
	}

out:
	flash_area_close(storage_area);
	if(ret){
		printf("Unable to save global config to flash\n");
	}

	return ret;
}

void mem_address_space_write(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space, uint32_t starting_address, void* data, int data_len){
	  if(address_space == 251){
		  if((starting_address + data_len) > 128){
				lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
				return;
		  }

		  uint8_t* global_config_raw = &global_config;
		  memcpy(global_config_raw + starting_address, data, data_len);
		  save_global_config();

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

	    // Re-init(or de-init) the tortoise outputs
	    for(int x = 0; x < 8; x++){
	    	tortoise_check_set_position(&lcc_tortoise_state.tortoises[x]);
	    }

	    // Re-init the events we consume
	    {
	    	struct lcc_event_context* evt_ctx = lcc_context_get_event_context(lcc_tortoise_state.lcc_context);
			lcc_event_clear_events(evt_ctx, LCC_EVENT_CONTEXT_CLEAR_EVENTS_CONSUMED);
			for(int x = 0; x < 8; x++){
				uint64_t* events_consumed = tortoise_events_consumed(&lcc_tortoise_state.tortoises[x]);
				lcc_event_add_event_consumed(evt_ctx, events_consumed[0]);
				lcc_event_add_event_consumed(evt_ctx, events_consumed[1]);
			}
	    }
	  }else if(address_space == 250){
		  // Global config
		  uint8_t* buffer_u8 = ((uint8_t*)&global_config.dcc_translation) + starting_address;

		  if(starting_address + data_len > 1){
				// trying to write too much memory
			    lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
				return;
		  }
		  memcpy(buffer_u8 + starting_address, data, data_len);

		  lcc_memory_respond_write_reply_ok(ctx, alias, address_space, starting_address);
		  save_global_config();
	  }else{
	    lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
	    return;
	  }
}

static void reboot(struct lcc_memory_context* ctx){
	save_tortoise_positions();
	save_switch_tracker();
	sys_reboot(SYS_REBOOT_COLD);
}

static void factory_reset(struct lcc_memory_context* ctx){
	memset(&lcc_tortoise_state.tortoise_config, 0, sizeof(lcc_tortoise_state.tortoise_config));
	uint64_t new_base_eventid = global_config.base_event_id + 16;
	memset(&global_config, 0, sizeof(global_config));
	global_config.base_event_id = new_base_eventid;

	for(int x = 0; x < 8; x++){
		lcc_tortoise_state.tortoise_config[x].BE_accessory_number = __builtin_bswap16(x + 1);
		lcc_tortoise_state.tortoise_config[x].startup_control = STARTUP_NORMAL;
		lcc_tortoise_state.tortoise_config[x].BE_event_id_closed = __builtin_bswap64(new_base_eventid++);
		lcc_tortoise_state.tortoise_config[x].BE_event_id_thrown = __builtin_bswap64(new_base_eventid++);
	}

	save_configs_to_flash();
	save_global_config();
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

static void accy_cb(struct dcc_packet_parser* parser, uint16_t accy_number, enum dcc_accessory_direction accy_dir){
	if(lcc_tortoise_state.button_control == BUTTON_CONTROL_DCC_ADDR_PROG){
		struct tortoise* current_tort = &lcc_tortoise_state.tortoises[lcc_tortoise_state.tort_output_current_idx];
		struct tortoise_config* config = current_tort->config;
		int current_accy_number = __builtin_bswap16(config->BE_accessory_number);
		if(current_accy_number != accy_number){
			config->BE_accessory_number = __builtin_bswap16(accy_number);
			config->control_type = CONTROL_DCC_ONLY;
			tortoise_enable_outputs(current_tort);
			k_sleep(K_MSEC(250));
			tortoise_disable_outputs(current_tort);
			save_configs_to_flash();
		}
		return;
	}

	for(int x = 0; x < 8; x++){
		int changed = tortoise_incoming_accy_command(&lcc_tortoise_state.tortoises[x],
				accy_number,
				accy_dir == ACCESSORY_NORMAL ? POSITION_NORMAL : POSITION_REVERSE);
		if(changed){
			set_tortoise_posistions_dirty();
		}
	}

	if(global_config.dcc_translation.do_dcc_translation){
		// Only do the switch tracking if we do not control this turnout directly
		for(int x = 0; x < 8; x++){
			if(tortoise_is_controlled_by_dcc_accessory(&lcc_tortoise_state.tortoises[x], accy_number)){
				return;
			}
		}
		switch_tracker_incoming_switch_command(accy_number, accy_dir);
	}
}

static void incoming_dcc(struct dcc_decoder* decoder, const uint8_t* packet_bytes, int len){
	lcc_tortoise_state.last_rx_dcc_msg = k_cycle_get_32();
}

static uint64_t load_lcc_id(){
	uint64_t ret = 0;

	const struct flash_area* lcc_storage_area = NULL;
	int id = FIXED_PARTITION_ID(lcc_partition);

	if(flash_area_open(id, &lcc_storage_area) < 0){
		return 1;
	}

	if(flash_area_read(lcc_storage_area, 0, &ret, sizeof(ret)) < 0){
		return 1;
	}

	// Check to see if the first 8 bytes are 0xFF.  If so, we assume the data is uninitialized
	const uint8_t mem[8] = {
			0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF
	};
	if(memcmp(&ret, mem, 8) == 0){
		uint64_t new_id = 0;
		int valid = 0;

		console_getline_init();

		while(!valid){
			printf("LCC ID blank!  Please input LCC ID as a single hex string followed by enter:\n");
			char id_buffer[32];
			char* console_data = console_getline();
			printf("console line: %s\n", console_data);
			new_id = strtoull(console_data, NULL, 16);
			lcc_node_id_to_dotted_format(new_id, id_buffer, sizeof(id_buffer));

			printf("New node ID will be %s.  Type 'y'<ENTER> to accept\n", id_buffer);
			console_data = console_getline();
			printf("response: %s\n", console_data);
			if(console_data[0] == 'y'){
				valid = 1;
				ret = new_id;
			}
		}

		flash_area_write(lcc_storage_area, 0, &ret, sizeof(ret));

		global_config.base_event_id = ret << 16;
		// Do a 'factory reset' to make sure everything is initialized
		factory_reset(NULL);
	}

out:
	flash_area_close(lcc_storage_area);
	return ret;
}

static void load_global_config(){
	const struct flash_area* storage_area = NULL;
	int id = FIXED_PARTITION_ID(global_partition);
	int ret = 0;

	if(flash_area_open(id, &storage_area) < 0){
		return 1;
	}

	if(flash_area_read(storage_area, 0, &global_config, sizeof(global_config)) < 0){
		ret = 1;
	}

out:
	flash_area_close(storage_area);
	if(ret){
		printf("Unable to load global config\n");
	}

	return ret;
}

static void blue_button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	int blue_value = gpio_pin_get_dt(&lcc_tortoise_state.blue_button);

	if(blue_value){
		lcc_tortoise_state.blue_button_press = k_cycle_get_32();
	}else{
		lcc_tortoise_state.blue_button_press_diff = k_cyc_to_ms_ceil32(k_cycle_get_32() - lcc_tortoise_state.blue_button_press);
		lcc_tortoise_state.blue_button_press = 0;
		lcc_tortoise_state.allow_new_command = 1;
	}
}

static void gold_button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	int gold_value = gpio_pin_get_dt(&lcc_tortoise_state.gold_button);

	if(gold_value){
		lcc_tortoise_state.gold_button_press = k_cycle_get_32();
	}else{
		lcc_tortoise_state.gold_button_press_diff = k_cyc_to_ms_ceil32(k_cycle_get_32() - lcc_tortoise_state.gold_button_press);
		lcc_tortoise_state.gold_button_press = 0;
	}
}

static void handle_button_press(){

	// First check the blue button for being held down, since that will get us into or out of the configuration mode
	if(lcc_tortoise_state.blue_button_press != 0 && lcc_tortoise_state.allow_new_command){
		int diff = k_cyc_to_ms_ceil32(k_cycle_get_32() - lcc_tortoise_state.blue_button_press);
		if(diff > 5000 && (lcc_tortoise_state.button_control == BUTTON_CONTROL_NORMAL)){
			lcc_tortoise_state.button_control = BUTTON_CONTROL_DCC_ADDR_PROG;
			lcc_tortoise_state.tort_output_current_idx = 0;
			lcc_tortoise_state.allow_new_command = 0;
			tortoise_disable_outputs(&lcc_tortoise_state.tortoises[lcc_tortoise_state.tort_output_current_idx]);
		}else if(diff > 2000 && (lcc_tortoise_state.button_control != BUTTON_CONTROL_NORMAL)){
			lcc_tortoise_state.button_control = BUTTON_CONTROL_NORMAL;
			lcc_tortoise_state.allow_new_command = 0;
			tortoise_enable_outputs(&lcc_tortoise_state.tortoises[lcc_tortoise_state.tort_output_current_idx]);
			k_wakeup(blue_blink);
			k_wakeup(gold_blink);
			k_wakeup(green_blink);
		}
	}

	if(lcc_tortoise_state.button_control == BUTTON_CONTROL_NORMAL){
		// Nothing more to do here, not in configuration mode
		return;
	}

	if(lcc_tortoise_state.blue_button_press_diff > 50 &&
			lcc_tortoise_state.blue_button_press_diff < 2000){
		lcc_tortoise_state.blue_button_press_diff = 0;
		if(lcc_tortoise_state.button_control == BUTTON_CONTROL_MAX){
			lcc_tortoise_state.button_control = BUTTON_CONTROL_DCC_ADDR_PROG;
			lcc_tortoise_state.tort_output_current_idx = 0;
			tortoise_disable_outputs(&lcc_tortoise_state.tortoises[lcc_tortoise_state.tort_output_current_idx]);
		}else{
			if(lcc_tortoise_state.button_control == BUTTON_CONTROL_DCC_ADDR_PROG){
				tortoise_enable_outputs(&lcc_tortoise_state.tortoises[lcc_tortoise_state.tort_output_current_idx]);
			}
			lcc_tortoise_state.button_control++;
		}
		k_wakeup(blue_blink);
	}

	if(lcc_tortoise_state.gold_button_press_diff != 0){
		lcc_tortoise_state.gold_button_press_diff = 0;
		if(lcc_tortoise_state.button_control == BUTTON_CONTROL_DCC_ADDR_PROG){
			tortoise_enable_outputs(&lcc_tortoise_state.tortoises[lcc_tortoise_state.tort_output_current_idx]);
			lcc_tortoise_state.tort_output_current_idx++;
			if(lcc_tortoise_state.tort_output_current_idx > 7){
				lcc_tortoise_state.tort_output_current_idx = 0;
			}
			tortoise_disable_outputs(&lcc_tortoise_state.tortoises[lcc_tortoise_state.tort_output_current_idx]);
		}
	}else if(lcc_tortoise_state.gold_button_press && lcc_tortoise_state.button_control == BUTTON_CONTROL_FACTORY_RESET){
		int diff = k_cyc_to_ms_ceil32(k_cycle_get_32() - lcc_tortoise_state.gold_button_press);
		if(diff > 2000){
			lcc_tortoise_state.blue_button_press = 0;
			lcc_tortoise_state.gold_button_press = 0;
			// Flash the blue and gold LEDs to indicate that the factory reset was done
			// This happens until the user releases the gold button
			k_thread_suspend(blue_blink);
			k_thread_suspend(gold_blink);

			factory_reset(NULL);
			while(1){
				gpio_pin_set_dt(&lcc_tortoise_state.blue_led, 1);
				gpio_pin_set_dt(&lcc_tortoise_state.gold_led, 0);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&lcc_tortoise_state.blue_led, 0);
				gpio_pin_set_dt(&lcc_tortoise_state.gold_led, 1);
				k_sleep(K_MSEC(100));
				int gold_value = gpio_pin_get_dt(&lcc_tortoise_state.gold_button);
				if(gold_value == 0){
					break;
				}
			}

			lcc_tortoise_state.button_control = BUTTON_CONTROL_NORMAL;
			lcc_tortoise_state.allow_new_command = 0;
			gpio_pin_set_dt(&lcc_tortoise_state.blue_led, 0);
			gpio_pin_set_dt(&lcc_tortoise_state.gold_led, 0);
			k_wakeup(green_blink);
			k_thread_resume(blue_blink);
			k_thread_resume(gold_blink);
		}
	}
}

static void main_loop(){
	struct k_poll_event poll_data[2];
	struct k_timer alias_timer;
	struct can_frame rx_frame;
	struct lcc_context* ctx = lcc_tortoise_state.lcc_context;

	k_poll_event_init(&poll_data[0],
			K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY,
			&rx_msgq);
	k_poll_event_init(&poll_data[1],
			K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY,
			&dcc_decode_ctx.readings);

	k_timer_init(&alias_timer, NULL, NULL);
	k_timer_start(&alias_timer, K_MSEC(250), K_NO_WAIT);

	while (1) {
		int rc = k_poll(poll_data, ARRAY_SIZE(poll_data), K_MSEC(250));
		if(poll_data[0].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
			// Receive a CAN frame if there is any
			while(k_msgq_get(&rx_msgq, &rx_frame, K_NO_WAIT) == 0){
				memset(&lcc_rx, 0, sizeof(lcc_rx));
				lcc_rx.can_id = rx_frame.id;
				lcc_rx.can_len = rx_frame.dlc;
				memcpy(lcc_rx.data, rx_frame.data, 8);

				lcc_context_incoming_frame(ctx, &lcc_rx);
			}
			poll_data[0].state = K_POLL_STATE_NOT_READY;
			lcc_tortoise_state.last_rx_can_msg = k_cycle_get_32();
		}else if(poll_data[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
			uint32_t timediff;
			while(k_msgq_get(&dcc_decode_ctx.readings, &timediff, K_NO_WAIT) == 0){
				if(dcc_decoder_polarity_changed(dcc_decode_ctx.dcc_decoder, timediff) == 1){
					dcc_decoder_pump_packet(dcc_decode_ctx.dcc_decoder);
				}
			}
			poll_data[1].state = K_POLL_STATE_NOT_READY;
		}

		if(k_timer_status_get(&alias_timer) > 0 &&
		    lcc_context_current_state(ctx) == LCC_STATE_INHIBITED){
		    int stat = lcc_context_claim_alias(ctx);
		    if(stat == LCC_ERROR_ALIAS_TX_NOT_EMPTY){
		    	k_timer_start(&alias_timer, K_MSEC(500), K_NO_WAIT);
		    	continue;
		    }
		    if(stat != LCC_OK){
		      // If we were unable to claim our alias, we need to generate a new one and start over
		      lcc_context_generate_alias(ctx);
		    }else{
		      printf("Claimed alias %X\n", lcc_context_alias(ctx) );

				// initialize our low power handling.
				// We do this right after we have an alias, since logically nothing will have changed
				// until we are actually able to process commands from some other device on the network.
				powerhandle_init();
				powerhandle_check_if_save_required();
		    }
		}

		handle_button_press();
	}
}

static int tx_queue_size_cb(struct lcc_context*){
	return k_msgq_num_free_get(&tx_msgq);
}

#if 0
static void check_eeprom(){
	const struct device* eeprom_dev = DEVICE_DT_GET(DT_NODELABEL(eeprom));
	int ret;

	for(int x = 0; x < 1; x++){
		uint8_t byte;
		ret = eeprom_read(eeprom_dev, x, &byte, 1);
		if(ret != 0){
			printf("can't read: %d\n", ret);
			break;
		}
		printf("0x%02X ", byte);
		if(x % 8 == 0){
			printf("\n");
		}
	}


	ret = eeprom_write(eeprom_dev, 0, "hi", 2);
	if(ret < 0){
		printf("unable to write\n");
	}else{
		printf("wrote\n");
	}
}
#endif

static void splash(){
	printf("LCC SMTC-8\n");
	printf("  Version: " VERSION_STR);
	printf("  Rev: P" CONFIG_BOARD_REVISION "\n");

	struct mcuboot_img_header versions[2];
	struct mcuboot_img_sem_ver semver[2] = {0};

	if(boot_read_bank_header(FIXED_PARTITION_ID(slot0_partition), &versions[0], sizeof(struct mcuboot_img_header)) == 0){
	  semver[0] = versions[0].h.v1.sem_ver;
	}
	if(boot_read_bank_header(FIXED_PARTITION_ID(slot1_partition), &versions[1], sizeof(struct mcuboot_img_header)) == 0){
	  semver[1] = versions[1].h.v1.sem_ver;
	}

	printf("  This slot version: %d.%d.%d\n", semver[0].major, semver[0].minor, semver[0].revision);
	printf("  Secondary slot version: %d.%d.%d\n", semver[1].major, semver[1].minor, semver[1].revision);
}

int main(void)
{
	int ret;

	// Sleep for a bit before we start to allow power to become stable.
	// This is probably not needed, but it shouldn't hurt.
	// This will also give other devices on the network a chance to come up.
	k_sleep(K_MSEC(250));

	splash();

	_Static_assert(sizeof(struct tortoise_config) == 32);

	if(lcc_tortoise_state_init() < 0){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return 0;
	}

	dcc_decoder_init(&dcc_decode_ctx);

	// Load the tortoise settings, and set the outputs to the correct valu depending on their startup settings
	int tortoise_load_stat = load_tortoise_settings();
	if(tortoise_load_stat == 1){
		printf("Error: Unable to load settings!\n");
		return 1;
	}else if(tortoise_load_stat == 2){
		printf("Chip is blank, initializing to default values...\n");
		factory_reset(NULL);
	}
//	for(int x = 0; x < 8; x++){
//		tortoise_set_position(&lcc_tortoise_state.tortoises[x], lcc_tortoise_state.tortoises[x].current_position);
//	}

	if (!device_is_ready(can_dev)) {
		printf("CAN: Device %s not ready.\n", can_dev->name);
		return 0;
	}

	ret = can_start(can_dev);
	if(ret != 0){
		printf("Can't start CAN\n");
		return 0;
	}

	init_can_txrx();

	lcc_tortoise_state.lcc_context = lcc_context_new();
	if(lcc_tortoise_state.lcc_context == NULL){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return -1;
	}

	dcc_decoder_set_packet_callback(dcc_decode_ctx.dcc_decoder, incoming_dcc);
	dcc_decoder_set_packet_parser(dcc_decode_ctx.dcc_decoder, dcc_decode_ctx.packet_parser);

	uint64_t lcc_id = load_lcc_id();
	load_global_config();
	struct lcc_context* ctx = lcc_tortoise_state.lcc_context;
	lcc_context_set_unique_identifer( ctx,
			lcc_id );
	lcc_context_set_simple_node_information(ctx,
			"Snowball Creek Electronics",
			"SMTC-8",
			"P" CONFIG_BOARD_REVISION,
			VERSION_STR);

	lcc_context_set_write_function( ctx, lcc_write_cb, tx_queue_size_cb );
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

	dcc_packet_parser_set_accessory_cb(dcc_decode_ctx.packet_parser, accy_cb);

	if(global_config.dcc_translation.do_dcc_translation){
		switch_tracker_init();
	}

	// Init our callbacks
	gpio_init_callback(&gold_button_cb_data, gold_button_pressed, BIT(lcc_tortoise_state.gold_button.pin));
	gpio_add_callback(lcc_tortoise_state.gold_button.port, &gold_button_cb_data);
	gpio_init_callback(&blue_button_cb_data, blue_button_pressed, BIT(lcc_tortoise_state.blue_button.pin));
	gpio_add_callback(lcc_tortoise_state.blue_button.port, &blue_button_cb_data);

//	check_eeprom();

	main_loop();

	return 0;
}
