/*
 * crossing-gate-structs.h
 *
 *  Created on: Jun 9, 2025
 *      Author: robert
 */

#ifndef LCC_CROSSING_GATE_SRC_CROSSING_GATE_STRUCTS_H_
#define LCC_CROSSING_GATE_SRC_CROSSING_GATE_STRUCTS_H_

#include <zephyr/drivers/gpio.h>
#include <stdint.h>

#define FLAG_POLARITY (0x01 << 0)
#define FLAG_USE_ANALOG (0x01 << 1)
#define FLAG_POLARITY_ACTIVE_HIGH 0
#define FLAG_POLARITY_ACTIVE_LOW 1

#define ROUTE_SWITCH_INPUT_NORMAL 0
#define ROUTE_SWITCH_INPUT_REVERSE 1

// Layout of the sensor input in EEPROM
struct sensor_input_eeprom{
	uint8_t gpio_number;
	uint8_t gpio_type;
	uint8_t polarity;
	uint8_t padding;
	uint16_t analog_value;
	uint64_t BE_event_id_on;
	uint64_t BE_event_id_off;
	uint16_t BE_debounce_on;
	uint16_t BE_debounce_off;
};

struct switch_input_eeprom{
	uint8_t gpio_number;
	uint8_t polarity;
	uint8_t route_posistion;
	uint8_t padding;
	uint64_t BE_event_id_normal;
	uint64_t BE_eevent_id_reverse;
};

struct sensor_config;
struct sensor_input{
	const struct gpio_dt_spec* sensor_gpio;
	struct sensor_config* config;
	int is_on;
};

struct switch_input_config;
struct switch_input{
	const struct gpio_dt_spec* switch_gpio;
	struct switch_input_config* config;
	int current_pos;
};

enum Direction{
	DIRECTION_UNKNOWN,
	DIRECTION_LTR,
	DIRECTION_RTL,
};

enum TrainLocation{
	LOCATION_UNOCCUPIED,
	LOCATION_PRE_ISLAND_OCCUPIED,
	LOCATION_ISLAND_OCCUPIED_INCOMING,
	LOCATION_ISLAND_OCCUPIED,
	LOCATION_POST_ISLAND_OCCUPIED_INCOMING,
	LOCATION_POST_ISLAND_OCCUPIED,
};

struct train{
	unsigned long last_seen_millis;
	enum Direction direction;
	enum TrainLocation location;
};

struct route_config;
struct route{
	struct route_config* config;
	struct sensor_input sensors[4];
	struct switch_input switch_inputs[8];
	struct train current_train;
	struct k_timer timeout;
	struct k_timer reactivation_timeout;
	uint16_t neighbor_routes_mask;
	int can_be_activated;
};

void sensor_input_init(struct sensor_input* input, struct gpio_dt_spec* gpio);

/**
 * Determine the current value of the sensor input.  Value will be either 0(for inactive) or 1(for active)
 */
int sensor_input_value(struct sensor_input* input);

/**
 * Get the raw value of the input(0 or 1 for digital, 0 to max for analog GPIO),
 * -1 if this sensor input is not going to a GPIO.
 * Does NOT take polarity into account.
 */
int sensor_input_raw_value(struct sensor_input* input);

/**
 * For the given sensor input, notify that input that an event ID has come in.
 * If this sensor input cares about the event ID, it will update itself.
 */
void sensor_input_handle_event(struct sensor_input* input, uint64_t event_id);

/**
 * Check to see if the sensor input is valid.  A valid sensor input has either a GPIO or both event IDs set.
 */
int sensor_input_valid(struct sensor_input* input);

void switch_input_init(struct switch_input* input, struct gpio_dt_spec* gpio);

int switch_input_enabled(struct switch_input* input);

/**
 * Determine the current value of the given switch.  Value will be either
 * 0(for normal) or 1(for reverse)
 */
int switch_input_value(struct switch_input* input);

void switch_input_handle_event(struct switch_input* input, uint64_t event_id);


#endif /* LCC_CROSSING_GATE_SRC_CROSSING_GATE_STRUCTS_H_ */
