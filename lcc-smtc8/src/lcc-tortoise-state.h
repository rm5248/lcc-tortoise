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
#include "switch-tracker.h"

struct lcc_context;
struct dcc_decoder;
struct dcc_packet_parser;

enum ButtonControlState{
	BUTTON_CONTROL_NORMAL = 0,
	BUTTON_CONTROL_DCC_ADDR_PROG = 1,
	BUTTON_CONTROL_FACTORY_RESET = 2,
	BUTTON_CONTROL_MAX = 2,
};

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

	// Internal state programming tracking
	uint32_t blue_button_press;
	uint32_t blue_button_press_diff;
	uint32_t gold_button_press;
	uint32_t gold_button_press_diff;
	uint8_t allow_new_command;
	enum ButtonControlState button_control;
	uint8_t tort_output_current_idx;

	uint8_t save_tortoise_pos_on_shutdown;
	uint8_t tortoise_pos_dirty;

	// Tracker for the current switch positions
	uint8_t switch_tracker_dirty;
	struct switch_tracker trackers[2048];

	// should we disable all commands to the outputs?
	uint8_t disable_outputs_voltage_out_of_range;
};

struct global_config{
	uint64_t device_id;
	uint8_t config_version;
};

struct dcc_address_translation_config{
#define DCC_TRANSLATION_ENABLE 1
#define DCC_TRANSLATION_ENABLE_AND_SAVE 2
	uint8_t do_dcc_translation;
};

struct global_config_data {
	char node_name[64];
	char node_description[64];
	uint64_t base_event_id;
	struct dcc_address_translation_config dcc_translation;
	uint8_t max_turnouts_at_once;
};

struct output_position_request{
	uint8_t output_number;
	uint8_t position;
};

extern struct lcc_tortoise_state lcc_tortoise_state;
extern struct global_config config;
extern struct global_config_data global_config;

int lcc_tortoise_state_init();

void save_tortoise_positions();

void set_tortoise_positions_dirty();

void save_switch_tracker();

void set_switch_tracker_dirty();

#endif /* LCC_TORTOISE_ZEPHYR_SRC_LCC_TORTOISE_STATE_H_ */
