/*
 * led-process-fade.c
 *
 *  Created on: Nov 21, 2025
 *      Author: robert
 */

#include <stdlib.h>
#include "led-funcs.h"
#include "../servo16-output-state.h"

static int fade_step_size(int diff){
	// Figure out the step size, assuming that we update every 60ms and that we are fading from
	// from 0 to 100% in 500ms
	const int num_updates_per_second = 1000 / 120;

	return diff / num_updates_per_second;
}

int led_process_fade_up(struct OutputState* state){
	// arg1 = percentage to go up to from our current brightness
	const int diff_between_vals = abs(state->led_arg1 - state->current_position);
	const int new_step = fade_step_size(diff_between_vals);

	if(state->current_position > state->led_arg1){
		// can't fade higher than our current value!
		return 0;
	}

	if(state->step_size != new_step){
		state->step_size = new_step;
	}

	state->current_position += state->step_size;
	if(state->current_position >= state->led_arg1){
		state->current_position = state->led_arg1;
		return 0;
	}

	return 1;
}

int led_process_fade_down(struct OutputState* state){
	// arg1 = percentage to fade down to from our current brightness
	const int diff_between_vals = abs(state->led_arg1 - state->current_position);
	const int new_step = fade_step_size(diff_between_vals);

	if(state->current_position < state->led_arg1){
		// can't fade lower than our current value!
		return 0;
	}

	if(state->step_size != new_step){
		state->step_size = new_step;
	}

	state->current_position -= state->step_size;
	if(state->current_position <= state->led_arg1){
		state->current_position = state->led_arg1;
		return 0;
	}

	return 1;
}
