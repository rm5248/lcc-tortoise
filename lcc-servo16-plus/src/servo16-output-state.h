/*
 * servo16-output-state.h
 *
 *  Created on: Nov 20, 2025
 *      Author: robert
 */

#ifndef LCC_SERVO16_PLUS_SRC_SERVO16_OUTPUT_STATE_H_
#define LCC_SERVO16_PLUS_SRC_SERVO16_OUTPUT_STATE_H_

#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

struct BoardOutput;

struct OutputState{
	const struct pwm_dt_spec pwm_output;
	struct k_timer output_change_timer;
	struct k_work output_change_work;
	struct BoardOutput* output_config;
	int current_position;
	int target_position;
	int step_size;
	int moving;

	// LED settings
	int current_led_func;
	int led_arg1;
	int led_arg2;
};

void output_state_init(struct OutputState* state, struct BoardOutput* output);

/**
 * Perform the action as specified by the given Event ID.
 * If the event ID is not consumed by this output, nothing happens.
 */
int output_state_perform_action(struct OutputState* state, uint8_t board_type, uint64_t event_id);

#endif /* LCC_SERVO16_PLUS_SRC_SERVO16_OUTPUT_STATE_H_ */
