/*
 * tortoise.c
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <stdlib.h>

#include "tortoise.h"
#include "lcc-event.h"

static uint64_t events[2];
static int tortoise_set_position_force(struct tortoise* tort, enum tortoise_position position);

static void tortoise_timer_expired(struct k_timer* timer){
	struct tortoise* tort = k_timer_user_data_get(timer);

	gpio_pin_set_dt(&tort->gpios[0], 0);
	gpio_pin_set_dt(&tort->gpios[1], 0);
}

int tortoise_init(struct tortoise* tort){
	int ret = 0;

	if (!gpio_is_ready_dt(&tort->gpios[0])) {
		return -1;
	}
	if (!gpio_is_ready_dt(&tort->gpios[1])) {
		return -1;
	}

	ret = gpio_pin_configure_dt(&tort->gpios[0], GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&tort->gpios[1], GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return -1;
	}

	k_timer_init(&tort->pulse_timer, tortoise_timer_expired, NULL);
	k_timer_user_data_set(&tort->pulse_timer, tort);

	return 0;
}

int tortoise_init_startup_position(struct tortoise* tort){
	if( tort->config->startup_control == STARTUP_REVERSE ){
		tort->current_position = POSITION_REVERSE;
	}else if( tort->config->startup_control == STARTUP_NORMAL ){
		tort->current_position = POSITION_NORMAL;
	}else{
		tort->current_position = tort->config->last_known_pos;
	}

	tortoise_set_position_force(tort, tort->current_position);

	return 0;
}

static int tortoise_handle_custom_event(struct tortoise* tort, uint64_t event_id){
	uint64_t throw_event_id = __builtin_bswap64(tort->config->BE_event_id_thrown);
	uint64_t close_event_id = __builtin_bswap64(tort->config->BE_event_id_closed);

	if(event_id == throw_event_id){
		return tortoise_set_position(tort, POSITION_REVERSE);
	}else if(event_id == close_event_id){
		return tortoise_set_position(tort, POSITION_NORMAL);
	}

	return 0;
}

int tortoise_incoming_event(struct tortoise* tort, uint64_t event_id){
	if(tort->config->control_type == CONTROL_LCC_CUSTOM_EVENT_ID){
		return tortoise_handle_custom_event(tort, event_id);
	}

	if( !lcc_event_id_is_accessory_address( event_id) ){
		return 0;
	}

	struct lcc_accessory_address addr;

	if( lcc_event_id_to_accessory_decoder_2040(event_id, &addr) < 0){
		return -1;
	}

	uint16_t accessory_number = __builtin_bswap16(tort->config->BE_accessory_number);
	if(accessory_number == addr.dcc_accessory_address){
		// This tortoise should change state
		return tortoise_set_position(tort, addr.active ? POSITION_NORMAL : POSITION_REVERSE);
	}

	return 0;
}

int tortoise_incoming_accy_command(struct tortoise* tort, uint16_t accy_number, enum tortoise_position pos){
	if(tort->config->control_type == CONTROL_LCC_ONLY ||
			tort->config->control_type == CONTROL_LCC_CUSTOM_EVENT_ID){
		return 0;
	}

	uint16_t accessory_number = __builtin_bswap16(tort->config->BE_accessory_number);
	if(accessory_number == accy_number){
		// This tortoise should change state
		return tortoise_set_position(tort, pos);
	}

	return 0;
}

int tortoise_set_position(struct tortoise* tort, enum tortoise_position position){
	if(tort->current_position == position){
		return 0;
	}

	return tortoise_set_position_force(tort, position);
}

static int tortoise_set_position_force(struct tortoise* tort, enum tortoise_position position){
	tort->current_position = position;
	gpio_pin_set_dt(&tort->gpios[0], 0);
	gpio_pin_set_dt(&tort->gpios[1], 0);

	if(position == POSITION_NORMAL){
		gpio_pin_set_dt(&tort->gpios[0], 0);
		gpio_pin_set_dt(&tort->gpios[1], 1);
	}else{
		gpio_pin_set_dt(&tort->gpios[0], 1);
		gpio_pin_set_dt(&tort->gpios[1], 0);
	}

	if(tort->config->output_type == OUTPUT_TYPE_PULSE){
		int ms = 0;
		switch(tort->config->pulse_len){
		default:
		case 0:
			ms = 200;
			break;
		case 1:
			ms = 400;
			break;
		case 2:
			ms = 800;
			break;
		case 3:
			ms = 1600;
			break;
		}

		printf("Pulse output %d ms\n", ms);
		k_timer_start(&tort->pulse_timer, K_MSEC(ms), K_NO_WAIT);
	}

	return 1;
}

uint64_t* tortoise_events_consumed(struct tortoise* tort){
	memset(events, 0, sizeof(events));

	if(tort->config->control_type == CONTROL_LCC_ONLY){
		struct lcc_accessory_address addr;
		addr.dcc_accessory_address = __builtin_bswap16(tort->config->BE_accessory_number);
		addr.active = 0;

		lcc_accessory_decoder_to_event_id_2040(&addr, &events[0]);

		addr.active = 1;
		lcc_accessory_decoder_to_event_id_2040(&addr, &events[1]);
	}else if(tort->config->control_type == CONTROL_LCC_CUSTOM_EVENT_ID){
		events[0] = __builtin_bswap64(tort->config->BE_event_id_closed);
		events[1] = __builtin_bswap64(tort->config->BE_event_id_thrown);
	}

	return events;
}

int tortoise_disable_outputs(struct tortoise* tort){
	gpio_pin_set_dt(&tort->gpios[0], 0);
	gpio_pin_set_dt(&tort->gpios[1], 0);

	return 0;
}

int tortoise_enable_outputs(struct tortoise* tort){
	enum tortoise_position position = tort->current_position;

	if(position == POSITION_NORMAL){
		gpio_pin_set_dt(&tort->gpios[0], 0);
		gpio_pin_set_dt(&tort->gpios[1], 1);
	}else{
		gpio_pin_set_dt(&tort->gpios[0], 1);
		gpio_pin_set_dt(&tort->gpios[1], 0);
	}
}

int tortoise_is_controlled_by_dcc_accessory(struct tortoise* tort, int accy_number){
	if(tort->config->control_type != CONTROL_LCC_CUSTOM_EVENT_ID ){
		int tortoise_accy_number = __builtin_bswap16(tort->config->BE_accessory_number);
		if(accy_number == tortoise_accy_number){
			return 1;
		}
	}

	return 0;
}

//int tortoise_config_to_bigendian(struct tortoise* tort, struct tortoise_config* out){
//	if(tort == NULL || out == NULL){
//		return -1;
//	}
//
//	memset(out, 0, sizeof(struct tortoise_config));
//	*out = tort->config;
//	out->event_id_thrown = __builtin_bswap64(tort->config.event_id_thrown);
//	out->event_id_closed = __builtin_bswap64(tort->config.event_id_closed);
//	out->accessory_number = __builtin_bswap16(tort->config.accessory_number);
//
//	return 0;
//}
//
//int tortoise_config_set_from_bigendian(struct tortoise* tort, struct tortoise_config* in){
//	if(tort == NULL || in == NULL){
//		return -1;
//	}
//
//	tort->config = *in;
//	tort->config.event_id_thrown = __builtin_bswap64(in->event_id_thrown);
//	tort->config.event_id_closed = __builtin_bswap64(in->event_id_closed);
//	tort->config.accessory_number = __builtin_bswap16(in->accessory_number);
//
//	return 0;
//}
