/*
 * crossing-gate-init.h
 *
 *  Created on: Jun 14, 2025
 *      Author: robert
 */

#ifndef LCC_CROSSING_GATE_SRC_CROSSING_GATE_INIT_H_
#define LCC_CROSSING_GATE_SRC_CROSSING_GATE_INIT_H_

#include <stdint.h>

void crossing_gate_init();

void crossing_gate_load_config();

/**
 * Set all values in RAM to their default settings.  EventIDs are
 * configured based off of base_event_id.
 */
void crossing_gate_set_default_values(uint64_t base_event_id);

#endif /* LCC_CROSSING_GATE_SRC_CROSSING_GATE_INIT_H_ */
