/*
 * pca9685_board.c
 *
 *  Created on: Oct 24, 2025
 *      Author: robert
 */
#include "pca9685_board.h"
#include "servo16-config.h"

static void pca9685_actuate(const struct pwm_dt_spec* spec, struct BoardOutput* output, struct BoardEvent* event){
	int LE_min_pulse = __builtin_bswap16(output->BE_min_pulse);
	int LE_max_pulse = __builtin_bswap16(output->BE_max_pulse);
	int diff = LE_max_pulse - LE_min_pulse;
	int val_to_change_by = (float)diff * (float)event->position / 100.0;
	int new_val = LE_min_pulse + val_to_change_by;

	printf("Actuate output %s to percent %d val: %d\n", output->description, event->position, new_val);

	pwm_set_pulse_dt(spec, PWM_USEC(new_val));
}

static void pca9685_led(const struct pwm_dt_spec* spec, struct BoardOutput* output, struct BoardEvent* event){
	const uint32_t period = PWM_MSEC(1);
	uint32_t new_pulse = output->default_led_function;

	switch(event->led_function){
	case LED_FUNC_OFF:
		new_pulse = 0;
		break;
	case LED_FUNC_STEADY_ON:
		new_pulse = PWM_MSEC(1) * (float)event->position / 200.0;
		break;
	}

	printf("Set LED %s to function %d(%d%%)\n", output->description, event->led_function, event->position);
	pwm_set_dt(spec, period, new_pulse);
}

int pca9685_board_handle_event(struct Board* board, uint64_t event_id){
	uint64_t BE_event_id = __builtin_bswap64(event_id);
	int handled = 0;

	for(int output = 0; output < 16; output++){
		for(int event_idx = 0; event_idx < 6; event_idx++){
			if(board->config->board_type == BOARD_TYPE_SERVO &&
					board->config->outputs[output].events[event_idx].BE_event_id == BE_event_id){
				// okay, we can make this guy do things
				handled = 1;
				pca9685_actuate(&board->pwm_outputs[output], &board->config->outputs[output], &board->config->outputs[output].events[event_idx]);
			}else if(board->config->board_type == BOARD_TYPE_LED &&
					board->config->outputs[output].events[event_idx].BE_event_id == BE_event_id){
				handled = 1;
				pca9685_led(&board->pwm_outputs[output], &board->config->outputs[output], &board->config->outputs[output].events[event_idx]);
			}
		}
	}

	return handled;
}
