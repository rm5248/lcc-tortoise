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
struct dcc_decoder;
struct dcc_packet_parser;

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
	struct tortoise_config tortoise_config[8];
	struct tortoise tortoises[8];

	struct lcc_context* lcc_context;
	struct dcc_decoder* dcc_decoder;
	struct dcc_packet_parser* packet_parser;
	struct gpio_callback dcc_cb_data;
	uint32_t prev_cycle;
	const struct device* dcc_counter;
};

struct global_config{
	uint64_t device_id;
	uint8_t config_version;
};

extern struct lcc_tortoise_state lcc_tortoise_state;
extern struct global_config config;

int lcc_tortoise_state_init();


#endif /* LCC_TORTOISE_ZEPHYR_SRC_LCC_TORTOISE_STATE_H_ */
