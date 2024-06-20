/*
 * tortoise.h
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#ifndef LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_
#define LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_

enum tortoise_position{
	NORMAL,
	REVERSED
};

enum StartupControl{
	STARTUP_THROWN,
	STARTUP_CLOSED,
	STARTUP_LAST_POSITION,
};

enum ControlMethods{
	CONTROL_LCC_ONLY,
	CONTROL_DCC_ONLY,
	CONTROL_LCC_DCC,
};

struct tortoise_config{
	uint64_t event_id_thrown;
	uint64_t event_id_closed;
	uint16_t accessory_number;
	uint8_t startup_control;
	uint8_t control_type;
	uint8_t last_known_pos;
	uint8_t reserved[11];
};

struct tortoise {
	const struct gpio_dt_spec gpios[2];
	enum tortoise_position current_position;
	struct tortoise_config config;
};

/**
 * Initialize the GPIOs for the tortoise
 */
int tortoise_init(struct tortoise* tort);

/**
 * Set the position that the tortoise should go to
 */
int tortoise_set_position(struct tortoise* tort, enum tortoise_position position);

#endif /* LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_ */
