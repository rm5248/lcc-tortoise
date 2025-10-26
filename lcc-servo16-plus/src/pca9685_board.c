/*
 * pca9685_board.c
 *
 *  Created on: Oct 24, 2025
 *      Author: robert
 */
#include "pca9685_board.h"
#include "servo16-config.h"

static void pca9685_actuate(const struct pwm_dt_spec* spec, struct ServoOutput* output, struct ServoEvent* event){
	int LE_min_pulse = __builtin_bswap16(output->BE_min_pulse);
	int LE_max_pulse = __builtin_bswap16(output->BE_max_pulse);
	int diff = LE_max_pulse - LE_min_pulse;
	int val_to_change_by = (float)diff * (float)event->position / 100.0;
	int new_val = LE_min_pulse + val_to_change_by;

	printf("Actuate output %s to percent %d val: %d\n", output->description, event->position, new_val);

	pwm_set_pulse_dt(spec, PWM_USEC(new_val));
}

int pca9685_board_handle_event(struct Board* board, uint64_t event_id){
	uint64_t BE_event_id = __builtin_bswap64(event_id);
	int handled = 0;

	for(int output = 0; output < 16; output++){
		for(int event_idx = 0; event_idx < 6; event_idx++){
			if(board->config->outputs[output].events[event_idx].BE_event_id == BE_event_id){
				// okay, we can make this guy do things
				handled = 1;
				pca9685_actuate(&board->pwm_outputs[output], &board->config->outputs[output], &board->config->outputs[output].events[event_idx]);
			}
		}
	}

	return handled;
}
