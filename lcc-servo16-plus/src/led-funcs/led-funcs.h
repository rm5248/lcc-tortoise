/*
 * led-funcs.h
 *
 *  Created on: Nov 21, 2025
 *      Author: robert
 */

#ifndef LCC_SERVO16_PLUS_SRC_LED_FUNCS_LED_FUNCS_H_
#define LCC_SERVO16_PLUS_SRC_LED_FUNCS_LED_FUNCS_H_

struct OutputState;

/**
 * A function that will be perodically called in order to change the LED state.
 *
 * @returns 0 if the processing is complete and should not run again, 1 otherwise
 */
typedef int(*led_process_function)(struct OutputState* state);

int led_process_on(struct OutputState* state);
int led_process_off(struct OutputState* state);
int led_process_pulse_slow(struct OutputState* state);
int led_process_pulse_medium(struct OutputState* state);
int led_process_pulse_fast(struct OutputState* state);
int led_process_fade_up(struct OutputState* state);
int led_process_fade_down(struct OutputState* state);

#endif /* LCC_SERVO16_PLUS_SRC_LED_FUNCS_LED_FUNCS_H_ */
