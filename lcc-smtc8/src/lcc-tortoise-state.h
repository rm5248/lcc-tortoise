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
#include "dcc-decode-stm32.h"

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
	const struct device* gpio_expander;

	// State information for LED blinking
	uint32_t last_rx_dcc_msg;
	uint32_t last_rx_can_msg;
	uint32_t last_tx_can_msg;
};

struct global_config{
	uint64_t device_id;
	uint8_t config_version;
};

extern struct lcc_tortoise_state lcc_tortoise_state;
extern struct global_config config;

int lcc_tortoise_state_init();


#endif /* LCC_TORTOISE_ZEPHYR_SRC_LCC_TORTOISE_STATE_H_ */
