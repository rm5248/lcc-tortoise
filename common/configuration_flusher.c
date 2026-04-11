/*
 * configuration_flusher.c
 *
 *  Created on: Apr 11, 2026
 *      Author: robert
 */


#include "configuration_flusher.h"
#include "lcc-memory-map.h"

static void configuration_flusher_work_cb(struct k_work* item){
	struct configuration_flusher* flusher = CONTAINER_OF(item, struct configuration_flusher, flush_work);

	if(flusher->write_segment_cb == NULL){
		return;
	}

	for(int x = 0; x < flusher->num_segments; x++){
		if(flusher->segment_dirty & BIT(x)){
			// segment is dirty, let's flush it
			flusher->write_segment_cb(flusher->segments[x].space);
		}
	}
	flusher->segment_dirty = 0;
}

static void configuration_flusher_timer_expired(struct k_timer* timer_id){
	struct configuration_flusher* flusher = CONTAINER_OF(timer_id, struct configuration_flusher, flush_timer);
	k_work_submit(&flusher->flush_work);
}

void configuration_flusher_init(struct configuration_flusher* flusher, const struct lcc_memory_segment* segment, int num_segments){
	memset(flusher, 0, sizeof(struct configuration_flusher));
	flusher->segments = segment;
	flusher->num_segments = num_segments;

	k_work_init(&flusher->flush_work, configuration_flusher_work_cb);
	k_timer_init(&flusher->flush_timer, configuration_flusher_timer_expired, NULL);
}

void configuration_flusher_set_dirty(struct configuration_flusher* flusher, int segment_number){
	int bitNum;
	for(bitNum = 0; bitNum < flusher->num_segments; bitNum++){
		if(flusher->segments[bitNum].space == segment_number){
			break;
		}
	}
	flusher->segment_dirty |= BIT(bitNum);
	k_timer_start(&flusher->flush_timer, K_MINUTES(6), K_NO_WAIT);
}

void configuration_flusher_set_write_callback(struct configuration_flusher* flusher, configuration_flusher_cb callback){
	flusher->write_segment_cb = callback;
}

void configuration_flusher_force_write_if_dirty(struct configuration_flusher* flusher){
	if(!flusher->write_segment_cb){
		return;
	}

	for(int x = 0; x < flusher->num_segments; x++){
		if(flusher->segment_dirty & BIT(x)){
			// segment is dirty, let's flush it
			flusher->write_segment_cb(flusher->segments[x].space);
		}
	}
	flusher->segment_dirty = 0;
}
