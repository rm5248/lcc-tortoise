/*
 * configuration_flusher.h
 *
 *  Created on: Apr 11, 2026
 *      Author: robert
 */

#ifndef CONFIGURATION_FLUSHER_H_
#define CONFIGURATION_FLUSHER_H_

#include <stdint.h>
#include <zephyr/kernel.h>

struct lcc_memory_segment;

typedef void(*configuration_flusher_cb)(int segment_number);

/**
 * Keeps track of what memory segments are dirty.  If they are,
 * periodically call a callback function to flush the data to flash.
 */
struct configuration_flusher{
	const struct lcc_memory_segment* segments;
	uint32_t segment_dirty;
	configuration_flusher_cb write_segment_cb;
	uint8_t num_segments;
	struct k_work flush_work;
	struct k_timer flush_timer;
};

void configuration_flusher_init(struct configuration_flusher* flusher, const struct lcc_memory_segment* segment, int num_segments);

void configuration_flusher_set_dirty(struct configuration_flusher* flusher, int segment_number);

void configuration_flusher_set_write_callback(struct configuration_flusher* flusher, configuration_flusher_cb callback);

void configuration_flusher_force_write_if_dirty(struct configuration_flusher* flusher);

#endif /* CONFIGURATION_FLUSHER_H_ */
