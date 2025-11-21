/*
 * servo16-output-state.c
 *
 *  Created on: Nov 20, 2025
 *      Author: robert
 */

#include "servo16-output-state.h"
#include "servo16-config.h"

static void output_state_led(struct OutputState* output_state, struct BoardEvent* event){
	int LE_arg1 = __builtin_bswap16(event->arg1);
	int LE_arg2 = __builtin_bswap16(event->arg2);
	int arg1_percent = PWM_MSEC(1) * (float)LE_arg1 / 1000.0;
	int arg2_percent = PWM_MSEC(1) * (float)LE_arg2 / 1000.0;

	output_state->moving = 2;
	output_state->current_led_func = event->led_function;
	output_state->led_arg1 = arg1_percent;
	output_state->led_arg2 = arg2_percent;

	switch(event->led_function){
	case LED_FUNC_OFF:
		output_state->current_position = 0;
		break;
	case LED_FUNC_STEADY_ON:
		output_state->current_position = arg1_percent;
		break;
	case LED_FUNC_PULSE_SLOW:
		output_state->current_position = arg1_percent;
		output_state->target_position = arg2_percent;
		output_state->step_size = 50;
		break;
	case LED_FUNC_PULSE_MED:
		output_state->current_position = arg1_percent;
		output_state->target_position = arg2_percent;
		output_state->step_size = 250;
		break;
	case LED_FUNC_PULSE_FAST:
		output_state->current_position = arg1_percent;
		output_state->target_position = arg2_percent;
		output_state->step_size = 5000;
		break;
	}

	printf("Set LED %s to function %d(%d%%)\n", output_state->output_config->description, event->led_function, LE_arg1);

	k_timer_stop(&output_state->output_change_timer);
	k_timer_start(&output_state->output_change_timer, K_NO_WAIT, K_MSEC(60));
//	pwm_set_dt(spec, period, new_pulse);
}

static int output_state_calculate_servo_step_size(int speed, int min_pulse, int max_pulse){
	// For the servos, we want to have several different steps:
	// slowest = full rotation in ??? TODO: need to figure out what a reasonable speed is
	// fastest = no periodic PWM change, just move directly to the given value
	// The output to the PWM is updated every 60ms
	const int diff = max_pulse - min_pulse;
	const int base_speed = 5;

	switch(speed){
	case 1:
		return base_speed;
	case 2:
		return base_speed * 5;
	case 3:
		return base_speed * 8;
	case 4:
		return base_speed * 16;
	case 5:
		return base_speed * 25;
	default:
		return diff;
	}
}

static void output_state_actuate_servo(struct OutputState* output_state, struct BoardEvent* event){
	int LE_min_pulse = __builtin_bswap16(output_state->output_config->BE_min_pulse);
	int LE_max_pulse = __builtin_bswap16(output_state->output_config->BE_max_pulse);
	int diff = LE_max_pulse - LE_min_pulse;
	int val_to_change_by = (float)diff * (float)event->arg1 / 1000.0;
	int new_val = LE_min_pulse + val_to_change_by;

	k_timer_stop(&output_state->output_change_timer);
	printf("Actuate output %s to percent %d val: %d\n", output_state->output_config->description, event->arg1, new_val);

	output_state->moving = 1;
	output_state->target_position = new_val;
	output_state->step_size = output_state_calculate_servo_step_size(output_state->output_config->servo_speed, LE_min_pulse, LE_max_pulse);
	if(output_state->target_position < output_state->current_position){
		output_state->step_size *= -1;
	}
	k_timer_start(&output_state->output_change_timer, K_NO_WAIT, K_MSEC(60));
}

static void output_state_timer_fired(struct k_timer* timer){
	struct OutputState* state =
			CONTAINER_OF(timer, struct OutputState, output_change_timer);

	k_work_submit(&state->output_change_work);
}

static void output_state_work_func(struct k_work* work){
	struct OutputState* state =
			CONTAINER_OF(work, struct OutputState, output_change_work);

	if(state->moving == 1 /* servo */){
		int LE_min_pulse = __builtin_bswap16(state->output_config->BE_min_pulse);
		int LE_max_pulse = __builtin_bswap16(state->output_config->BE_max_pulse);

		state->current_position += state->step_size;
		if(state->step_size > 0 &&
				state->current_position >= LE_max_pulse){
			k_timer_stop(&state->output_change_timer);
			state->current_position = LE_max_pulse;
			state->moving = 0;
		}else if(state->step_size < 0 &&
				state->current_position <= LE_min_pulse){
			k_timer_stop(&state->output_change_timer);
			state->current_position = LE_min_pulse;
			state->moving = 0;
		}

		pwm_set_pulse_dt(&state->pwm_output, PWM_USEC(state->current_position));
	}else if(state->moving == 2 /* LED */){
		if(state->current_led_func == LED_FUNC_OFF ||
				state->current_led_func == LED_FUNC_STEADY_ON){
			k_timer_stop(&state->output_change_timer);
			state->moving = 0;
		}else{
			state->current_position += state->step_size;
			if(state->current_position >= state->led_arg1){
				state->step_size *= -1;
				state->current_position = state->led_arg1;
			}else if(state->current_position <= state->led_arg2){
				state->step_size *= -1;
				state->current_position = state->led_arg2;
			}
		}
		pwm_set_dt(&state->pwm_output, PWM_MSEC(1), state->current_position);
	}
}

void output_state_init(struct OutputState* state, struct BoardOutput* output){
	state->output_config = output;
	state->moving = 0;
	state->step_size = 0;

	k_timer_init(&state->output_change_timer, output_state_timer_fired, NULL);
	k_work_init(&state->output_change_work, output_state_work_func);
}

int output_state_perform_action(struct OutputState* state, uint8_t board_type, uint64_t event_id){
	uint64_t BE_event_id = __builtin_bswap64(event_id);
	int handled = 0;

	for(int event_idx = 0; event_idx < 6; event_idx++){
		struct BoardEvent* board_event = &state->output_config->events[event_idx];
		if(board_type == BOARD_TYPE_SERVO &&
				board_event->BE_event_id == BE_event_id){
			// okay, we can make this guy do things
			handled = 1;
//				pca9685_actuate_servo(&board->pwm_outputs[output], &board->config->outputs[output], &board->config->outputs[output].events[event_idx]);
			output_state_actuate_servo(state, board_event);
		}else if(board_type == BOARD_TYPE_LED &&
				board_event->BE_event_id == BE_event_id){
			handled = 1;
			output_state_led(state, board_event);
//				pca9685_led(&board->pwm_outputs[output], &board->config->outputs[output], &board->config->outputs[output].events[event_idx]);
		}
	}

	return handled;
}
