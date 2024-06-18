/*
 * lcc-tortoise-state.h
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#ifndef LCC_TORTOISE_ZEPHYR_SRC_LCC_TORTOISE_STATE_H_
#define LCC_TORTOISE_ZEPHYR_SRC_LCC_TORTOISE_STATE_H_

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>

#include "tortoise.h"

struct lcc_context;

/**
 * The global state struct for our application
 */
struct lcc_tortoise_state{
	const struct gpio_dt_spec green_led;
	const struct gpio_dt_spec blue_led;
	const struct gpio_dt_spec gold_led;
	const struct gpio_dt_spec blue_button;
	const struct gpio_dt_spec gold_button;
	const struct gpio_dt_spec dcc_signal;
	const struct device* fram;
	struct tortoise tortoises[8];

	struct lcc_context* lcc_context;
};

extern struct lcc_tortoise_state lcc_tortoise_state;

int lcc_tortoise_state_init();


#endif /* LCC_TORTOISE_ZEPHYR_SRC_LCC_TORTOISE_STATE_H_ */
