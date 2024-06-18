/*
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/device.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"
#include "dcc-decode.h"

#include "lcc.h"
#include "lcc-common.h"
#include "lcc-event.h"

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000


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
K_THREAD_STACK_DEFINE(dcc_signal_stack, 128);

struct k_thread poll_state_thread_data;
struct k_thread dcc_thread_data;
CAN_MSGQ_DEFINE(rx_msgq, 2);

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
	if( !lcc_event_id_is_accessory_address( event_id) ){
		return;
	}

	struct lcc_accessory_address addr;

	if( lcc_event_id_to_accessory_decoder_2040(event_id, &addr) < 0){
		return;
	}

	printf("accy %d\n", addr.dcc_accessory_address);

	for(int x = 0; x < 8; x++){
		struct tortoise* current_tort = &lcc_tortoise_state.tortoises[x];
		if(current_tort->accessoryNumber == addr.dcc_accessory_address){
			// This tortoise should change state
			printf("tortoise change\n");
			tortoise_set_position(current_tort, addr.active ? REVERSED : NORMAL);
		}
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

static void check_eeprom(){
	uint32_t values[2];

	int rc = eeprom_read(lcc_tortoise_state.fram, 1024, &values, sizeof(values));
	if(rc < 0){
		printf("Can't read eeprom: %d\n", rc);
		return;
	}

	printf("values:\n");
	printf("  0x%08x\n", values[0]);
	printf("  0x%08x\n", values[1]);

	values[0] = values[0] + 1;
	values[1] = values[1] * 2;
	rc = eeprom_write(lcc_tortoise_state.fram, 1024, &values, sizeof(values));
	if(rc < 0){
		printf("Can't write eeprom: %d\n", rc);
		return;
	}
}

static void load_tortoise_settings(){
	// Tortoise config data is 32 bytes long
	struct tortoise_config current_config;
	uint32_t offset = 0;

	_Static_assert(sizeof(current_config) <= 32);

	for(int x = 0; x < 8; x++){
		int rc = eeprom_read(lcc_tortoise_state.fram, offset, &current_config, sizeof(current_config));
		if(rc < 0){
			printf("Can't read eeprom: %d\n", rc);
			return;
		}

		lcc_tortoise_state.tortoises[x].thrown_event_id = current_config.event_id_thrown;
		lcc_tortoise_state.tortoises[x].closed_event_id = current_config.event_id_closed;
		lcc_tortoise_state.tortoises[x].accessoryNumber = current_config.dcc_switch_number;
		lcc_tortoise_state.tortoises[x].control_method = current_config.control_type;
		lcc_tortoise_state.tortoises[x].startup_control = current_config.startup_control;

		if( lcc_tortoise_state.tortoises[x].startup_control == STARTUP_THROWN ){
			lcc_tortoise_state.tortoises[x].position = REVERSED;
		}else if( lcc_tortoise_state.tortoises[x].startup_control == STARTUP_CLOSED ){
			lcc_tortoise_state.tortoises[x].position = NORMAL;
		}else{
			lcc_tortoise_state.tortoises[x].position = current_config.last_known_pos;
		}
	}
}

int main(void)
{
	int ret;
	bool led_state = true;
	k_tid_t rx_tid, get_state_tid;
	k_tid_t dcc_thread;

	if(lcc_tortoise_state_init() < 0){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize!\n");
		return 0;
	}

	dcc_decode.gpio_pin = &lcc_tortoise_state.dcc_signal;
	dcc_decode.led_pin = &lcc_tortoise_state.blue_led;

	// Load the tortoise settings, and set the outputs to the correct valu depending on their startup settings
	load_tortoise_settings();
	for(int x = 0; x < 8; x++){
		tortoise_set_position(&lcc_tortoise_state.tortoises[x], lcc_tortoise_state.tortoises[x].position);

		// accy numbers 1-8 for now
		lcc_tortoise_state.tortoises[x].accessoryNumber = x + 1;
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

	dcc_thread = k_thread_create(&dcc_thread_data,
			dcc_signal_stack,
			K_THREAD_STACK_SIZEOF(dcc_signal_stack),
			dcc_decoder_thread, NULL, NULL, NULL,
			STATE_POLL_THREAD_PRIORITY, 0,
			K_NO_WAIT);
	if (!dcc_thread) {
		printf("ERROR spawning dcc thread\n");
	}

	can_set_state_change_callback(can_dev, state_change_callback, &state_change_work);

	init_rx_queue();

	check_eeprom();

	lcc_tortoise_state.lcc_context = lcc_context_new();
	if(lcc_tortoise_state.lcc_context == NULL){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize!\n");
		return 0;
	}

	struct lcc_context* ctx = lcc_tortoise_state.lcc_context;
	lcc_context_set_unique_identifer( ctx,
			0x020202000012ll );

	lcc_context_set_write_function( ctx, lcc_write_cb, NULL );
	struct lcc_event_context* evt_ctx = lcc_event_new(ctx);

	lcc_event_set_incoming_event_function(evt_ctx, incoming_event);
	lcc_event_set_listen_all_events(evt_ctx, 1);

	int stat = lcc_context_generate_alias( ctx );
	if(stat < 0){
		printf("error: can't gen alias: %d\n", stat);
		return 0;
	}
	uint64_t claim_alias_time = k_uptime_get() + 250;
	uint64_t blinky_time = claim_alias_time;
	struct can_frame rx_frame;

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
	}
	return 0;
}
