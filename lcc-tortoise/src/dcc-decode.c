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
#include "lcc-tortoise-state.h"
#include "dcc-decoder.h"

static void polarity_change(int value){
	const uint32_t curr_cycle = k_cycle_get_32();
	const uint32_t cycle_time_usec = k_cyc_to_us_floor32(curr_cycle - lcc_tortoise_state.prev_cycle);
	lcc_tortoise_state.prev_cycle = curr_cycle;

	gpio_pin_set_dt(&lcc_tortoise_state.gold_led, value);

	dcc_decoder_polarity_changed(lcc_tortoise_state.dcc_decoder, cycle_time_usec);
}

void dcc_decoder_thread(void*, void*, void*){
	int prev = 0;

	while(1){
		int state = gpio_pin_get_dt(&lcc_tortoise_state.dcc_signal);
		if( state != prev ){
			prev = state;
			polarity_change(state);
		}
		k_yield();
	}
}
