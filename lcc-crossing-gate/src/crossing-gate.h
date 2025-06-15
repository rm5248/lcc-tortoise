/*
 * crossing-gate.h
 *
 *  Created on: Jun 9, 2025
 *      Author: robert
 */

#ifndef LCC_CROSSING_GATE_SRC_CROSSING_GATE_H_
#define LCC_CROSSING_GATE_SRC_CROSSING_GATE_H_

#include <zephyr/drivers/gpio.h>

#include "crossing-gate-structs.h"

#define NUM_ROUTES 16

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
	const struct gpio_dt_spec power;
	unsigned long time_millis;
	enum BellRingType ring_type;
};

/**
 * Holds the global instance data for the crossing gate controller
 */
struct crossing_gate{
	enum GateFlashState gate_flash_state;
	unsigned long timeout_millis;
	struct route crossing_routes[NUM_ROUTES];

	// The specific IO on the board
	const struct gpio_dt_spec inputs[8];
	const struct tortoise tortoise_control[2];
	struct bell bell;
	const struct gpio_dt_spec led[3];
};

extern struct crossing_gate crossing_gate_state;
extern const k_tid_t gate_blink;

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

#endif /* LCC_CROSSING_GATE_SRC_CROSSING_GATE_H_ */
