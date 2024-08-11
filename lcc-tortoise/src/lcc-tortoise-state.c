/*
 * lcc-tortoise-state.c
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"

struct lcc_tortoise_state lcc_tortoise_state = {
		.green_led = GPIO_DT_SPEC_GET(DT_ALIAS(greenled), gpios),
		.blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(blueled), gpios),
		.gold_led = GPIO_DT_SPEC_GET(DT_ALIAS(goldled), gpios),
		.blue_button = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_switch), gpios),
		.gold_button = GPIO_DT_SPEC_GET(DT_NODELABEL(gold_switch), gpios),
		.dcc_signal = GPIO_DT_SPEC_GET(DT_NODELABEL(dcc_pin), gpios),
		.fram = DEVICE_DT_GET(DT_ALIAS(fram)),
		.tortoises = {
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 1)
						},
						.current_position = CLOSED
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise1), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise1), gpios, 1)
						},
						.current_position = CLOSED
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise2), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise2), gpios, 1)
						},
						.current_position = CLOSED
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise3), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise3), gpios, 1)
						},
						.current_position = CLOSED
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise4), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise4), gpios, 1)
						},
						.current_position = CLOSED
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise5), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise5), gpios, 1)
						},
						.current_position = CLOSED
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise6), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise6), gpios, 1)
						},
						.current_position = CLOSED
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise7), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise7), gpios, 1)
						},
						.current_position = CLOSED
				},
		}
};

static int init_led(const struct gpio_dt_spec* led){
	if (!gpio_is_ready_dt(led)) {
		return -1;
	}

	if(gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE) < 0){
		return -1;
	}

	return 0;
}

static int init_button(const struct gpio_dt_spec* button){
	if (!gpio_is_ready_dt(button)) {
		return -1;
	}

	if(gpio_pin_configure_dt(button, GPIO_INPUT) < 0){
		return -1;
	}

	// TODO callbacks on button press

	return 0;
}

int lcc_tortoise_state_init(){
	if(init_led(&lcc_tortoise_state.green_led) < 0){
		return -1;
	}
	if(init_led(&lcc_tortoise_state.gold_led) < 0){
		return -1;
	}
	if(init_led(&lcc_tortoise_state.blue_led) < 0){
		return -1;
	}
	if(init_button(&lcc_tortoise_state.blue_button) < 0){
		return -1;
	}
	if(init_button(&lcc_tortoise_state.gold_button) < 0){
		return -1;
	}
	if(init_button(&lcc_tortoise_state.dcc_signal) < 0){
		return -1;
	}

	for(int x = 0; x < 8; x++){
		lcc_tortoise_state.tortoises[x].config = &lcc_tortoise_state.tortoise_config[x];

		if(tortoise_init(&lcc_tortoise_state.tortoises[x]) < 0){
			return -1;
		}
	}

	return 0;
}
