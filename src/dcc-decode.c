/*
 * dcc-decode.c
 *
 *  Created on: Jun 17, 2024
 *      Author: robert
 */
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "dcc-decode.h"

struct dcc_decoder dcc_decode;

void dcc_decoder_thread(void*, void*, void*){
	uint32_t num_times = 0;
	int prev = 0;

	while(1){
		int state = gpio_pin_get_dt(dcc_decode.gpio_pin);
		if( state != prev ){
			prev = state;
			num_times++;
		}

		if( num_times % 100 == 0 ){
			gpio_pin_toggle_dt(dcc_decode.led_pin);
		}
	}
}
