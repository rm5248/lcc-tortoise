/*
 * crossing-gate.h
 *
 *  Created on: Jun 9, 2025
 *      Author: robert
 */

#ifndef LCC_CROSSING_GATE_SRC_CROSSING_GATE_H_
#define LCC_CROSSING_GATE_SRC_CROSSING_GATE_H_

#include <zephyr/drivers/gpio.h>
#include <assert.h>

#include "crossing-gate-structs.h"

#define NUM_ROUTES 16

enum ConfigMode {
	CONFIG_MODE_NORMAL = 0,
	CONFIG_MODE_GRIDCONNECT = 1,
	CONFIG_MODE_FACTORY_RESET = 2,
	CONFIG_MODE_MAX = 2,
};

enum GateFlashState{
	FLASH_OFF,
	FLASH_ON,
};

enum BellRingType{
	BELL_RING_NONE = 0,
	BELL_RING_LOWER,
	BELL_RING_RAISE,
	BELL_RING_LOWER_AND_RAISE,
	BELL_RING_WHILE_FLASH,
};

struct tortoise {
	const struct gpio_dt_spec gpios[2];
};

struct bell {
	const struct gpio_dt_spec enable;
	unsigned long time_millis;
	enum BellRingType ring_type;
};

/**
 * Node info - segment 251
 */
struct node_info_segment {
	char name[64];
	char description[64];
};

struct sensor_config {
	char sensor_name[27];
	uint8_t sensor_enabled;
	uint8_t sensor_type;
	uint8_t sensor_input;
	uint8_t polarity;
	uint8_t reserved;
	uint64_t BE_event_sensor_on;
	uint64_t BE_event_sensor_off;
};
_Static_assert(sizeof(struct sensor_config) == 48);

struct switch_input_config {
	char sensor_name[27];
	uint8_t switch_enabled;
	uint8_t switch_input_type;
	uint8_t swithc_input;
	uint8_t polarity;
	uint8_t position_for_route;
	uint64_t BE_event_switch_normal;
	uint64_t BE_event_switch_reversed;
};
_Static_assert(sizeof(struct switch_input_config) == 48);

struct route_config {
	char route_name[63];
	char route_enabled;
	struct sensor_config inputs[4];
	struct switch_input_config switch_inputs[8];
};
_Static_assert(sizeof(struct route_config) == 640);

/**
 * Route info - segment 253
 */
struct routes_segment {
	struct route_config all_routes[NUM_ROUTES];
};
_Static_assert(sizeof(struct routes_segment) == 10240);

struct general_input {
	char input_name[24];
	uint64_t BE_input_activated_event;
	uint64_t BE_input_deactivated_event;
};
_Static_assert(sizeof(struct general_input) == 40);

/**
 * General events - segment 252
 */
struct general_events_segment {
	uint64_t BE_gates_active_event;
	uint64_t BE_gates_inactive_event;
	uint64_t BE_manual_gates_activate_event;
	struct general_input inputs[8];
};
_Static_assert(sizeof(struct general_events_segment) == 344);

/**
 * General config - segment 250
 */
struct general_config_segment {
	uint8_t timeout;
	uint8_t bell_behavior;
	uint16_t bell_ring_time;
	uint32_t reserved[13];

	// Hidden configuration values
	uint64_t base_event_id;
};
_Static_assert(sizeof(struct general_config_segment) == 64);
#define SIZEOF_GENERAL_CONFIG (4)


struct pwm_output_config{
	uint8_t usage;
	uint8_t polarity;
	uint8_t reserved[14];
};
_Static_assert(sizeof(struct pwm_output_config) == 16);

/**
 * PWM configuration - segment 249
 */
struct pwm_output_segment {
	struct pwm_output_config pwm_configs[6];
};
_Static_assert(sizeof(struct pwm_output_segment) == 16 * 6);

/**
 * Holds the global instance data for the crossing gate controller
 */
struct crossing_gate{
	enum GateFlashState gate_flash_state;
	unsigned long timeout_millis;
	struct route crossing_routes[NUM_ROUTES];
	struct lcc_context* lcc_ctx;

	// The specific IO on the board
	const struct gpio_dt_spec inputs[8];
	const struct tortoise tortoise_control[2];
	struct bell bell;
	const struct gpio_dt_spec led[3];
	// LEDs for the crossing gates
	const struct device* led_pwm;

	const struct gpio_dt_spec tortoise_power;

	// Buttons
	const struct gpio_dt_spec blue_button;
	const struct gpio_dt_spec gold_button;

	// Button press tracking
	uint32_t blue_button_press;
	uint32_t blue_button_press_diff;
	uint32_t gold_button_press;
	uint32_t gold_button_press_diff;
	enum ConfigMode config_mode;
	uint8_t gridconnect_mode;

	int pin_changemsgq_buffer[10];
	struct k_msgq pin_change_msgq;

	// Segment 250
	struct general_config_segment general_config;
	// Segment 251
	struct node_info_segment node_info;
	// Segment 252
	struct general_events_segment general_events;
	// Segment 253
	struct routes_segment routes_config;
	//Segmetn 249
	struct pwm_output_segment pwm_config;
};

extern struct crossing_gate crossing_gate_state;

/**
 * Call this method whenever data should be processed(e.g. an input changes).
 * This should also be called periodically to update the internal state.
 */
void crossing_gate_update();

/**
 * Call this method when an event comes in.
 */
void crossing_gate_incoming_event(uint64_t event_id);

void crossing_gate_raise_arms();

void crossing_gate_lower_arms();

void crossing_gate_timer_expired(struct k_timer* timer_id);

#endif /* LCC_CROSSING_GATE_SRC_CROSSING_GATE_H_ */
