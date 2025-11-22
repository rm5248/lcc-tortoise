/*
 * led-process-onoff.c
 *
 *  Created on: Nov 21, 2025
 *      Author: robert
 */

#include "led-funcs.h"
#include "../servo16-output-state.h"

int led_process_off(struct OutputState* state){
	state->current_position = 0;
	return 0;
}

int led_process_on(struct OutputState* state){
	state->current_position = state->led_arg1;
	return 0;
}
