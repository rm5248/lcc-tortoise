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
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/console/console.h>
#include <zephyr/dfu/mcuboot.h>

#include "crossing-gate.h"
#include "crossing-gate-init.h"

#include "lcc.h"
#include "lcc-common.h"
#include "lcc-event.h"
#include "lcc-memory.h"

/*
 * Address Space / storage information
 * 253 = basic config space.  This is stored in RAM during run-time.
 *   If it changes, it is written to the config_partition location
 * 251 = node name/description.  This is stored in the global_partition
 * 249 = firmware version header information
 */

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static struct gpio_callback blue_button_cb_data;
static struct gpio_callback gold_button_cb_data;

K_THREAD_STACK_DEFINE(tx_stack, 512);
struct k_thread tx_thread_data;
k_tid_t tx_thread_tid;
CAN_MSGQ_DEFINE(rx_msgq, 55);
CAN_MSGQ_DEFINE(tx_msgq, 12);

static uint32_t last_tx_can_msg = 0;
static uint32_t last_rx_can_msg = 0;
const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led), gpios);
const struct gpio_dt_spec gold_led = GPIO_DT_SPEC_GET(DT_NODELABEL(gold_led), gpios);
const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(DT_NODELABEL(green_led), gpios);

static void blink_led_green(){
	int x = 0;
	while(1){
		gpio_pin_set_dt(&green_led, 1);
		k_sleep(K_MSEC(100));
		gpio_pin_set_dt(&green_led, 0);
		k_sleep(K_MSEC(1500));

//		if(x++ % 2){
//			crossing_gate_raise_arms();
//		}else{
//			crossing_gate_lower_arms();
//		}
	}
}

static void blink_led_blue(){
	while(1){
		uint64_t diff = k_cycle_get_32() - last_rx_can_msg;

		if(k_cyc_to_ms_ceil32(diff) < 25){
			gpio_pin_set_dt(&blue_led, 1);
			k_sleep(K_MSEC(50));
			gpio_pin_set_dt(&blue_led, 0);
		}
		k_sleep(K_MSEC(10));
	}
}

static void blink_led_gold(){
	while(1){
		uint64_t diff = k_cycle_get_32() - last_tx_can_msg;

		if(k_cyc_to_ms_ceil32(diff) < 25){
			gpio_pin_set_dt(&gold_led, 1);
			k_sleep(K_MSEC(50));
			gpio_pin_set_dt(&gold_led, 0);
		}
		k_sleep(K_MSEC(10));
	}
}

K_THREAD_DEFINE(green_blink, 256, blink_led_green, NULL, NULL, NULL,
		7, 0, 0);
K_THREAD_DEFINE(blue_blink, 256, blink_led_blue, NULL, NULL, NULL,
		7, 0, 0);
K_THREAD_DEFINE(gold_blink, 256, blink_led_gold, NULL, NULL, NULL,
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

			last_tx_can_msg = k_cycle_get_32();
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

void mem_address_space_information_query(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space){
	if(address_space == 251){
		lcc_memory_respond_information_query(ctx, alias, 1, address_space, 64 + 64, 0, 0);
	}else if(address_space == 253){
		// basic config space
		// 32 bytes * 8 tortoises
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
//	if(address_space == 251){
//		// This space is what we use for node name/description
//		uint8_t* buffer = global_config.node_name + starting_address;
//
//		if(starting_address + read_count > 128){
//			// trying to read too much memory
//			lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
//			return;
//		}
//
//		lcc_memory_respond_read_reply_ok(ctx, alias, address_space, starting_address, buffer, read_count);
//	  }else if(address_space == 253){
//		// Basic config space
//		if(starting_address + read_count > (sizeof(lcc_tortoise_state.tortoise_config))){
//		  // trying to read too much memory
//		  lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
//		  return;
//		}
//
//	    uint8_t* config_as_u8 = (uint8_t*)lcc_tortoise_state.tortoise_config;
//
//	    lcc_memory_respond_read_reply_ok(ctx, alias, address_space, starting_address, config_as_u8 + starting_address, read_count);
//	  }else if(address_space == 250){
//		  // Global config
//		  uint8_t* buffer = &global_config.dcc_translation + starting_address;
//
//		  if(starting_address + read_count > 1){
//				// trying to read too much memory
//				lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
//				return;
//			}
//
//			lcc_memory_respond_read_reply_ok(ctx, alias, address_space, starting_address, buffer, read_count);
//	  }else if(address_space == 249){
//		  struct mcuboot_img_header versions[2];
//		  struct mcuboot_img_sem_ver semver[2] = {0};
//
//		  if(boot_read_bank_header(FIXED_PARTITION_ID(slot0_partition), &versions[0], sizeof(struct mcuboot_img_header)) == 0){
//			  semver[0] = versions[0].h.v1.sem_ver;
//		  }
//		  if(boot_read_bank_header(FIXED_PARTITION_ID(slot1_partition), &versions[1], sizeof(struct mcuboot_img_header)) == 0){
//			  semver[1] = versions[1].h.v1.sem_ver;
//		  }
//
//		  if(starting_address + read_count > sizeof(semver)){
//				// trying to read too much memory
//				lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
//				return;
//			}
//
//		  uint8_t* buffer = ((uint8_t*)&semver) + starting_address;
//			lcc_memory_respond_read_reply_ok(ctx, alias, address_space, starting_address, buffer, read_count);
//	  }
}

int save_configs_to_flash(){
//	int ret = 0;
//
//	const struct flash_area* storage_area = NULL;
//	int id = FIXED_PARTITION_ID(config_partition);
//
//	if(flash_area_open(id, &storage_area) < 0){
//		return 1;
//	}
//
//	ret = flash_area_erase(storage_area, 0, sizeof(lcc_tortoise_state.tortoise_config));
//	if(ret){
//		goto out;
//	}
//
//	if(flash_area_write(storage_area, 0, &lcc_tortoise_state.tortoise_config, sizeof(lcc_tortoise_state.tortoise_config)) < 0){
//		ret = 1;
//	}
//
//out:
//	flash_area_close(storage_area);
//	if(ret){
//		printf("Unable to save configs to flash\n");
//	}
//
//	return ret;
}

static int save_global_config(){
//	const struct flash_area* storage_area = NULL;
//	int id = FIXED_PARTITION_ID(global_partition);
//	int ret = 0;
//
//	if(flash_area_open(id, &storage_area) < 0){
//		return 1;
//	}
//
//	ret = flash_area_erase(storage_area, 0, sizeof(global_config));
//	if(ret){
//		goto out;
//	}
//
//	if(flash_area_write(storage_area, 0, &global_config, sizeof(global_config)) < 0){
//		ret = 1;
//	}
//
//out:
//	flash_area_close(storage_area);
//	if(ret){
//		printf("Unable to save global config to flash\n");
//	}
//
//	return ret;
}

void mem_address_space_write(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space, uint32_t starting_address, void* data, int data_len){
//	  if(address_space == 251){
//		  if((starting_address + data_len) > 128){
//				lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
//				return;
//		  }
//
//		  uint8_t* global_config_raw = &global_config;
//		  memcpy(global_config_raw + starting_address, data, data_len);
//		  save_global_config();
//
//		  lcc_memory_respond_write_reply_ok(ctx, alias, address_space, starting_address);
//	  }else if(address_space == 253){
//		  if((starting_address + data_len) > sizeof(lcc_tortoise_state.tortoise_config)){
//			    lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
//			    return;
//		  }
//		  uint8_t* buffer_u8 = (uint8_t*)lcc_tortoise_state.tortoise_config;
//
//		  memcpy(buffer_u8 + starting_address, data, data_len);
//
//	    // Write out to non-volatile memory
//	    save_configs_to_flash();
//
//	    lcc_memory_respond_write_reply_ok(ctx, alias, address_space, starting_address);
//
//	    // Re-init(or de-init) the tortoise outputs
//	    for(int x = 0; x < 8; x++){
//	    	tortoise_check_set_position(&lcc_tortoise_state.tortoises[x]);
//	    }
//
//	    // Re-init the events we consume
//	    {
//	    	struct lcc_event_context* evt_ctx = lcc_context_get_event_context(lcc_tortoise_state.lcc_context);
//			lcc_event_clear_events(evt_ctx, LCC_EVENT_CONTEXT_CLEAR_EVENTS_CONSUMED);
//			for(int x = 0; x < 8; x++){
//				uint64_t* events_consumed = tortoise_events_consumed(&lcc_tortoise_state.tortoises[x]);
//				lcc_event_add_event_consumed(evt_ctx, events_consumed[0]);
//				lcc_event_add_event_consumed(evt_ctx, events_consumed[1]);
//			}
//	    }
//	  }else if(address_space == 250){
//		  // Global config
//		  uint8_t* buffer_u8 = ((uint8_t*)&global_config.dcc_translation) + starting_address;
//
//		  if(starting_address + data_len > 1){
//				// trying to write too much memory
//			    lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
//				return;
//		  }
//		  memcpy(buffer_u8 + starting_address, data, data_len);
//
//		  lcc_memory_respond_write_reply_ok(ctx, alias, address_space, starting_address);
//		  save_global_config();
//	  }else{
//	    lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
//	    return;
//	  }
}

static void reboot(struct lcc_memory_context* ctx){
	sys_reboot(SYS_REBOOT_COLD);
}

static void factory_reset(struct lcc_memory_context* ctx){
//	memset(&lcc_tortoise_state.tortoise_config, 0, sizeof(lcc_tortoise_state.tortoise_config));
//	uint64_t new_base_eventid = global_config.base_event_id + 16;
//	memset(&global_config, 0, sizeof(global_config));
//	global_config.base_event_id = new_base_eventid;
//
//	for(int x = 0; x < 8; x++){
//		lcc_tortoise_state.tortoise_config[x].BE_accessory_number = __builtin_bswap16(x + 1);
//		lcc_tortoise_state.tortoise_config[x].startup_control = STARTUP_LAST_POSITION;
//		lcc_tortoise_state.tortoise_config[x].BE_event_id_closed = __builtin_bswap64(new_base_eventid++);
//		lcc_tortoise_state.tortoise_config[x].BE_event_id_thrown = __builtin_bswap64(new_base_eventid++);
//	}
//
//	save_configs_to_flash();
//	save_global_config();
}

static enum lcc_consumer_state query_consumer_state(struct lcc_context* ctx, uint64_t event_id){
//	for(int x = 0; x < 8; x++){
//		uint64_t* events = tortoise_events_consumed(&lcc_tortoise_state.tortoises[x]);
//
//		if(event_id == events[0]){
//			// reverse/off
//			if(lcc_tortoise_state.tortoises[x].current_position == POSITION_REVERSE){
//				return LCC_CONSUMER_VALID;
//			}
//			return LCC_CONSUMER_INVALID;
//		}else if(event_id == events[1]){
//			// normal/on
//			if(lcc_tortoise_state.tortoises[x].current_position == POSITION_NORMAL){
//				return LCC_CONSUMER_VALID;
//			}
//			return LCC_CONSUMER_INVALID;
//		}
//	}

	return LCC_CONSUMER_UNKNOWN;
}

static uint64_t load_lcc_id(){
	uint64_t ret = 0;
	int do_reboot = 0;

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

		// Do our birthing checks to validate that the outputs and inputs are working
//		smtc8_birthing();

		while(!valid){
			printf("LCC ID blank!  Please input lower 2 bytes of LCC ID as a single hex string followed by enter:\n");
			char id_buffer[32];
			char* console_data = console_getline();
			printf("console line: %s\n", console_data);
			new_id = strtoull(console_data, NULL, 16);
			new_id = new_id & 0xFFFF;
			new_id = new_id | (0x02020200ull << 16);
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

//		global_config.base_event_id = ret << 16;
		// Do a 'factory reset' to make sure everything is initialized
		factory_reset(NULL);
		do_reboot = 1;
	}

out:
	flash_area_close(lcc_storage_area);

	if(do_reboot){
		printf("Birthing complete, rebooting...\n");
		sys_reboot(SYS_REBOOT_COLD);
	}

	return ret;
}

static void load_global_config(){
//	const struct flash_area* storage_area = NULL;
//	int id = FIXED_PARTITION_ID(global_partition);
//	int ret = 0;
//
//	if(flash_area_open(id, &storage_area) < 0){
//		return 1;
//	}
//
//	if(flash_area_read(storage_area, 0, &global_config, sizeof(global_config)) < 0){
//		ret = 1;
//	}
//
//out:
//	flash_area_close(storage_area);
//	if(ret){
//		printf("Unable to load global config\n");
//	}
//
//	return ret;
}

static void blue_button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
}

static void gold_button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
}

static void main_loop(struct lcc_context* ctx){
	struct k_poll_event poll_data[2];
	struct k_timer alias_timer;
	struct can_frame rx_frame;
	struct lcc_can_frame lcc_rx;

	k_poll_event_init(&poll_data[0],
			K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY,
			&rx_msgq);
//	k_poll_event_init(&poll_data[1],
//			K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
//			K_POLL_MODE_NOTIFY_ONLY,
//			&dcc_decode_ctx.readings);

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
			last_rx_can_msg = k_cycle_get_32();
		}else if(poll_data[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
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
		    }
		}
	}
}

static int tx_queue_size_cb(struct lcc_context*){
	return k_msgq_num_free_get(&tx_msgq);
}

static void splash(){
#define VERSION_STR "1.0"
	printf("LCC Crossing Gate Controller\n");
	printf("  Version: " VERSION_STR);
	printf("  Rev: R" CONFIG_BOARD_REVISION "\n");

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

static int init_led(const struct gpio_dt_spec* led){
	if (!gpio_is_ready_dt(led)) {
		return -1;
	}

	if(gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE) < 0){
		return -1;
	}

	return 0;
}

// node 63
#define LED_PWM_NODE_ID DT_COMPAT_GET_ANY_STATUS_OKAY(pwm_leds)
// node 64
#define LED_PWM_FOO DT_ALIAS(gate_led1)
// node 64
#define LED_PWM_FOO2 DT_NODELABEL(out_led1)

void pwm_foobar(){
	const struct device* led_pwm;
	led_pwm = DEVICE_DT_GET(LED_PWM_NODE_ID);
//	printf("node path: %s\n", DT_NODE_PATH(LED_PWM_NODE_ID));
//	printf("node path2: %s\n", DT_NODE_PATH(LED_PWM_FOO2));
//	const struct pwm_dt_spec spec = PWM_DT_SPEC_GET(DT_NODELABEL(led_out1));
//	const struct device* led_pwm = spec.dev;

	if(!led_pwm){
		printf("bad led pwm!\n");
		return;
	}

	if(!device_is_ready(led_pwm)){
		printf("device not ready\n");
	}

	led_on(led_pwm, 0);
	k_sleep(K_MSEC(1000));
	led_off(led_pwm, 0);

	int bright_val = 0;
	int dir = 1;
	while(true){
		led_set_brightness(led_pwm, 0, bright_val);
		led_set_brightness(led_pwm, 1, 100 - bright_val);
		led_set_brightness(led_pwm, 2, bright_val);

		if(dir){
			bright_val++;
			if(bright_val >= 100){
				dir = 0;
			}
		}else{
			bright_val--;
			if(bright_val <= 0){
				dir = 1;
			}
		}

		k_sleep(K_MSEC(5));
	}
}

void do_servo(){
	static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(DT_NODELABEL(servo));
	static const uint32_t min_pulse = DT_PROP(DT_NODELABEL(servo), min_pulse);
	static const uint32_t max_pulse = DT_PROP(DT_NODELABEL(servo), max_pulse);

	#define STEP PWM_USEC(100)

	enum direction {
		DOWN,
		UP,
	};

	uint32_t pulse_width = min_pulse;
		enum direction dir = UP;
		int ret;

		printf("Servomotor control\n");

		if (!pwm_is_ready_dt(&servo)) {
			printk("Error: PWM device %s is not ready\n", servo.dev->name);
			return;
		}

		while (1) {
			printf("set pulse width %dns\n", pulse_width/1000);
			ret = pwm_set_pulse_dt(&servo, pulse_width);
			if (ret < 0) {
				printk("Error %d: failed to set pulse width\n", ret);
				return;
			}

			if (dir == DOWN) {
				if (pulse_width <= min_pulse) {
					dir = UP;
					pulse_width = min_pulse;
				} else {
					pulse_width -= STEP;
				}
			} else {
				pulse_width += STEP;

				if (pulse_width >= max_pulse) {
					dir = DOWN;
					pulse_width = max_pulse;
				}
			}

			k_sleep(K_SECONDS(1));
//			k_sleep(K_MSEC(500));
		}
}

int main(void)
{
	int ret;

	// Sleep for a bit before we start to allow power to become stable.
	// This is probably not needed, but it shouldn't hurt.
	// This will also give other devices on the network a chance to come up.
	k_sleep(K_MSEC(250));

	splash();
	init_led(&green_led);
	init_led(&blue_led);
	init_led(&gold_led);

//	pwm_foobar();
	do_servo();

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
	crossing_gate_init();

	struct lcc_context* ctx = lcc_context_new();
	if(ctx == NULL){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return -1;
	}

	uint64_t lcc_id = load_lcc_id();
	load_global_config();
	lcc_context_set_unique_identifer( ctx,
			lcc_id );
	lcc_context_set_simple_node_information(ctx,
			"Snowball Creek Electronics",
			"Crossing Gate Controller",
			"R" CONFIG_BOARD_REVISION,
			VERSION_STR);

	lcc_context_set_write_function( ctx, lcc_write_cb, tx_queue_size_cb );
	struct lcc_event_context* evt_ctx = lcc_event_new(ctx);

	lcc_event_set_incoming_event_function(evt_ctx, incoming_event);
	lcc_event_add_event_consumed_query_fn(evt_ctx, query_consumer_state);

	lcc_datagram_context_new(ctx);
	struct lcc_memory_context* mem_ctx = lcc_memory_new(ctx);
	const char* cdi = "<cdi></cdi>";
	lcc_memory_set_cdi(mem_ctx, cdi, sizeof(cdi), 0);
	lcc_memory_set_memory_functions(mem_ctx,
	    mem_address_space_information_query,
	    mem_address_space_read,
	    mem_address_space_write);
	lcc_memory_set_reboot_function(mem_ctx, reboot);
	lcc_memory_set_factory_reset_function(mem_ctx, factory_reset);

//	firmware_upgrade_init(ctx);

	int stat = lcc_context_generate_alias( ctx );
	if(stat < 0){
		printf("error: can't gen alias: %d\n", stat);
		return 0;
	}

	// Init our callbacks
//	gpio_init_callback(&gold_button_cb_data, gold_button_pressed, BIT(lcc_tortoise_state.gold_button.pin));
//	gpio_add_callback(lcc_tortoise_state.gold_button.port, &gold_button_cb_data);
//	gpio_init_callback(&blue_button_cb_data, blue_button_pressed, BIT(lcc_tortoise_state.blue_button.pin));
//	gpio_add_callback(lcc_tortoise_state.blue_button.port, &blue_button_cb_data);

	main_loop(ctx);

	return 0;
}
