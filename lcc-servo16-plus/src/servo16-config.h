/*
 * servo16-config.h
 *
 *  Created on: Oct 18, 2025
 *      Author: robert
 */

#ifndef LCC_SERVO16_PLUS_SRC_SERVO16_CONFIG_H_
#define LCC_SERVO16_PLUS_SRC_SERVO16_CONFIG_H_

#include <stdint.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "servo16-output-state.h"

struct lcc_event_context;

enum LEDFunctions{
	LED_FUNC_OFF,
	LED_FUNC_STEADY_ON,
	// TODO figure out the expected hz for the pulse functions
	LED_FUNC_PULSE_SLOW,
	LED_FUNC_PULSE_MED,
	LED_FUNC_PULSE_FAST,
};

enum BoardType{
	BOARD_TYPE_SERVO,
	BOARD_TYPE_LED,
};

/**
 * Board Event information
 *
 * Read from EPROM.
 */
struct BoardEvent{
	uint64_t BE_event_id;
	uint16_t arg1;
	uint16_t arg2;
	uint8_t led_function;
	uint8_t reserved[3];
};
_Static_assert(sizeof(struct BoardEvent) == 16, "bad size for BoardEvent");

/**
 * Board Output configuration
 *
 * Read from EPROM
 */
struct BoardOutput{
	char description[32];

	// Servo specific settings
	uint16_t BE_min_pulse;
	uint16_t BE_max_pulse;
	uint8_t servo_speed;

	uint8_t default_startup_event;

	uint8_t reserved[10];
	struct BoardEvent events[6];
};
_Static_assert(sizeof(struct BoardOutput) == 144, "bad size for BoardOutput");

/**
 * BoardConfig.
 *
 * Read from EPROM
 */
struct BoardConfig{
	uint8_t address;
	uint8_t board_type;
	uint8_t res1;
	uint8_t res2;
	uint32_t res3;
//	uint8_t reserved[3];
	struct BoardOutput outputs[16];
};
_Static_assert(sizeof(struct BoardConfig) == 2304 + 8, "bad size for BoardConfig");

struct Board{
	// Configuration that comes from EPROM
	struct BoardConfig* config;

	// Runtime state information
	struct OutputState output_state[16];
};

struct Servo16PlusState{
	struct BoardConfig pwm_boards_config[4];
	struct Board boards[4];
	char name[64];
	char description[64];
	const struct gpio_dt_spec oe_pin;
	const struct gpio_dt_spec green_led;
	const struct gpio_dt_spec blue_led;
	const struct gpio_dt_spec gold_led;
};

extern struct Servo16PlusState servo16_state;

int init_state(uint64_t board_id);

void output_enable(void);

int save_config_to_flash();
int load_config_from_flash();
void add_all_events_consumed(struct lcc_event_context* ctx);

#endif /* LCC_SERVO16_PLUS_SRC_SERVO16_CONFIG_H_ */
