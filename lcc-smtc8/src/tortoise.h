/*
 * tortoise.h
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#ifndef LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_
#define LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_

#include <stdint.h>
#include <zephyr/kernel.h>

#define OUTPUT_TYPE_ALWAYS_ON 0
#define OUTPUT_TYPE_PULSE 1

#define PULSE_TIME_200MS 0
#define PULSE_TIME_400MS 1
#define PULSE_TIME_800MS 2
#define PULSE_TIME_1600MS 3

enum tortoise_position{
	POSITION_NORMAL,
	POSITION_REVERSE,
};

enum StartupControl{
	STARTUP_NORMAL,
	STARTUP_REVERSE,
	STARTUP_LAST_POSITION,
};

enum ControlMethods{
	CONTROL_LCC_ONLY,
	CONTROL_DCC_ONLY,
	CONTROL_LCC_DCC,
	CONTROL_LCC_CUSTOM_EVENT_ID,
	CONTROL_DISABLE,
};

struct tortoise_config{
	uint64_t BE_event_id_thrown;
	uint64_t BE_event_id_closed;
	uint16_t BE_accessory_number;
	uint8_t startup_control;
	uint8_t control_type;
	uint8_t last_known_pos;
	uint8_t output_type;
	uint8_t pulse_len;
	uint8_t reserved[9];
};

struct tortoise {
	const struct gpio_dt_spec gpios[2];
	enum tortoise_position current_position;
	struct tortoise_config* config;
	struct k_timer pulse_timer;
};

/**
 * Initialize the GPIOs for the tortoise
 */
int tortoise_init(struct tortoise* tort);

int tortoise_init_startup_position(struct tortoise* tort);

/**
 *
 * @return 1 if the position changed, 0 if position did not change
 */
int tortoise_incoming_event(struct tortoise* tort, uint64_t event_id);

/**
 *
 * @return 1 if the position changed, 0 if position did not change
 */
int tortoise_incoming_accy_command(struct tortoise* tort, uint16_t accy_number, enum tortoise_position pos);

/**
 * Set the position that the tortoise should go to
 *
 * @return 1 if the position changed, 0 otherwise
 */
int tortoise_set_position(struct tortoise* tort, enum tortoise_position position);

/**
 * Get the events consumed by this tortoise.  Returns an array of 2 elements.
 * The first element is the reverse/off event, the second element is the normal/on event.
 */
uint64_t* tortoise_events_consumed(struct tortoise* tort);

int tortoise_disable_outputs(struct tortoise* tort);

int tortoise_enable_outputs(struct tortoise* tort);

/**
 * Check to see if this tortoise is controlled by the given DCC accessory number.
 * This function takes into account LCC events as well, so that even if the tortoise
 * is controlled only over LCC it will return 'true' if the givenn accessory number
 * matches the standard LCC event ID.
 */
int tortoise_is_controlled_by_accessory(struct tortoise* tort, int accy_number);

/**
 * Check the current position of the tortoise, turning it on or disabling as appropriate.
 */
int tortoise_check_set_position(struct tortoise* tort);

/**
 * Convert the configuration for this tortoise to big-endian, putting it in 'out'
 */
//int tortoise_config_to_bigendian(struct tortoise* tort, struct tortoise_config* out);

/**
 * Configure this tortoise settings according to 'in', which is assumed to be in big-endian format
 */
//int tortoise_config_set_from_bigendian(struct tortoise* tort, struct tortoise_config* in);

#endif /* LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_ */
