/*
 * tortoise.h
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#ifndef LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_
#define LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_

#include <stdint.h>

enum tortoise_position{
	POSITION_REVERSE,
	POSITION_NORMAL
};

enum StartupControl{
	STARTUP_REVERSE,
	STARTUP_NORMAL,
	STARTUP_LAST_POSITION,
};

enum ControlMethods{
	CONTROL_LCC_ONLY,
	CONTROL_DCC_ONLY,
	CONTROL_LCC_DCC,
};

struct tortoise_config{
	uint64_t BE_event_id_thrown;
	uint64_t BE_event_id_closed;
	uint16_t BE_accessory_number;
	uint8_t startup_control;
	uint8_t control_type;
	uint8_t last_known_pos;
	uint8_t reserved[11];
};

struct tortoise {
	const struct gpio_dt_spec gpios[2];
	enum tortoise_position current_position;
	struct tortoise_config* config;
};

/**
 * Initialize the GPIOs for the tortoise
 */
int tortoise_init(struct tortoise* tort);

int tortoise_init_startup_position(struct tortoise* tort);

int tortoise_incoming_event(struct tortoise* tort, uint64_t event_id);

/**
 * Set the position that the tortoise should go to
 */
int tortoise_set_position(struct tortoise* tort, enum tortoise_position position);

/**
 * Get the events consumed by this tortoise.  Returns an array of 2 elements.
 * The first element is the reverse/off event, the second element is the normal/on event.
 */
uint64_t* tortoise_events_consumed(struct tortoise* tort);

int tortoise_disable_outputs(struct tortoise* tort);

/**
 * Convert the configuration for this tortoise to big-endian, putting it in 'out'
 */
//int tortoise_config_to_bigendian(struct tortoise* tort, struct tortoise_config* out);

/**
 * Configure this tortoise settings according to 'in', which is assumed to be in big-endian format
 */
//int tortoise_config_set_from_bigendian(struct tortoise* tort, struct tortoise_config* in);

#endif /* LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_ */
