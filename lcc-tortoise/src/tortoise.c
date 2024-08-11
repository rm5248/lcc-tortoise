/*
 * tortoise.c
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "tortoise.h"
#include "lcc-event.h"

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

	return 0;
}

int tortoise_init_startup_position(struct tortoise* tort){
	if( tort->config->startup_control == STARTUP_THROWN ){
		tort->current_position = THROWN;
	}else if( tort->config->startup_control == STARTUP_CLOSED ){
		tort->current_position = CLOSED;
	}else{
		tort->current_position = tort->config->last_known_pos;
	}

	return 0;
}

int tortoise_incoming_event(struct tortoise* tort, uint64_t event_id){
	if( !lcc_event_id_is_accessory_address( event_id) ){
		return 0;
	}

	struct lcc_accessory_address addr;

	if( lcc_event_id_to_accessory_decoder_2040(event_id, &addr) < 0){
		return -1;
	}

	uint16_t accessory_number = __builtin_bswap16(tort->config->accessory_number);
	if(accessory_number == addr.dcc_accessory_address){
		// This tortoise should change state
		tortoise_set_position(tort, addr.active ? THROWN : CLOSED);
	}

	return 0;
}

int tortoise_set_position(struct tortoise* tort, enum tortoise_position position){
	gpio_pin_set_dt(&tort->gpios[0], 0);
	gpio_pin_set_dt(&tort->gpios[1], 0);

	if(position == CLOSED){
		gpio_pin_set_dt(&tort->gpios[0], 0);
		gpio_pin_set_dt(&tort->gpios[1], 1);
	}else{
		gpio_pin_set_dt(&tort->gpios[0], 1);
		gpio_pin_set_dt(&tort->gpios[1], 0);
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
