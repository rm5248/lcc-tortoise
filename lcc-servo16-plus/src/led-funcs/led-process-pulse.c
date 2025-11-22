/*
 * led-process-pulse-slow.c
 *
 *  Created on: Nov 21, 2025
 *      Author: robert
 */
#include <stdlib.h>
#include "led-funcs.h"
#include "../servo16-output-state.h"

enum type{
	SLOW,
	MED,
	FAST
};

static int step_size(int type, int diff){
	// Figure out the step size, assuming that we update every 60ms
	const int num_updates_per_second = 1000 / 60;
	switch(type){
	case SLOW:
		return diff / (num_updates_per_second / 2);
	case MED:
		return diff / (num_updates_per_second / 4);
	case FAST:
		return diff / (num_updates_per_second / 8);
	}
}

static int led_pulse(struct OutputState* state, int step){
	const int diff_between_vals = abs(state->led_arg1 - state->led_arg2);
	const int new_step = step_size(step, diff_between_vals);

	if(abs(state->step_size) != new_step){
		state->step_size = new_step;
	}

	state->current_position += state->step_size;
	if(state->current_position >= state->led_arg1){
		state->step_size *= -1;
		state->current_position = state->led_arg1;
	}else if(state->current_position <= state->led_arg2){
		state->step_size *= -1;
		state->current_position = state->led_arg2;
	}

	return 1;
}

int led_process_pulse_slow(struct OutputState* state){
	return led_pulse(state, SLOW);
}

int led_process_pulse_medium(struct OutputState* state){
	return led_pulse(state, MED);
}

int led_process_pulse_fast(struct OutputState* state){
	return led_pulse(state, FAST);
}
