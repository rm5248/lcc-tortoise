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

struct ServoEvent{
	uint64_t BE_event_id;
	uint8_t position;
	uint8_t reserved[3];
};

struct ServoOutput{
	char description[32];
	uint16_t BE_min_pulse;
	uint16_t BE_max_pulse;
	uint8_t reserved[12];
	struct ServoEvent events[6];
};

struct BoardConfig{
//	uint8_t address;
//	uint8_t reserved[3];
	struct ServoOutput outputs[16];
};

struct Board{
	const struct pwm_dt_spec pwm_outputs[16];
	struct BoardConfig* config;
};

struct Servo16PlusState{
	struct BoardConfig pwm_boards_config[4];
	struct Board boards[4];
	char name[64];
	char description[64];
	const struct gpio_dt_spec oe_pin;
	const struct gpio_dt_spec green_led;
};

extern struct Servo16PlusState servo16_state;

int init_state(uint64_t board_id);

void output_enable(void);

#endif /* LCC_SERVO16_PLUS_SRC_SERVO16_CONFIG_H_ */
