/*
 * switch-tracker.c
 *
 *  Created on: Dec 16, 2024
 *      Author: robert
 */
#include <zephyr/storage/flash_map.h>

#include "switch-tracker.h"
#include "lcc-tortoise-state.h"

#include "lcc.h"
#include "lcc-common.h"
#include "lcc-event.h"

static enum lcc_producer_state switch_producer_state(struct lcc_context* ctx, uint64_t event_id){
	struct lcc_accessory_address addr = {0};
	enum lcc_producer_state ret = LCC_PRODUCER_UNKNOWN;

	if( lcc_event_id_to_accessory_decoder_2040(event_id, &addr) < 0){
		goto out;
	}

	if(addr.dcc_accessory_address < 0 || addr.dcc_accessory_address > 2048){
		goto out;
	}

	switch(lcc_tortoise_state.trackers[addr.dcc_accessory_address].current_pos){
	case SWITCH_UNKNOWN:
		break;
	case SWITCH_NORMAL:
		if(addr.active){
			ret = LCC_PRODUCER_VALID;
		}else{
			ret = LCC_PRODUCER_INVALID;
		}
		break;
	case SWITCH_REVERSED:
		if(!addr.active){
			ret = LCC_PRODUCER_VALID;
		}else{
			ret = LCC_PRODUCER_INVALID;
		}
	}

out:
	return ret;
}

void switch_tracker_init(){
	memset(lcc_tortoise_state.trackers, 0, sizeof(lcc_tortoise_state.trackers));

	struct lcc_accessory_address address;
	uint64_t event_id;
	struct lcc_event_context* evt_ctx = lcc_context_get_event_context(lcc_tortoise_state.lcc_context);

	lcc_event_add_event_produced_transaction_start(evt_ctx);

	for(int x = 0; x < 2040; x++){
		address.active = 1;
		address.dcc_accessory_address = x;
		address.reserved = 0;

		if(lcc_accessory_decoder_to_event_id_2040(&address, &event_id) == LCC_OK){
			lcc_event_add_event_produced(evt_ctx, event_id);
		}

		address.active = 0;
		if(lcc_accessory_decoder_to_event_id_2040(&address, &event_id) == LCC_OK){
			lcc_event_add_event_produced(evt_ctx, event_id);
		}
	}

	lcc_event_add_event_produced_transaction_end(evt_ctx);

	lcc_event_add_event_produced_query_fn(evt_ctx, switch_producer_state);

	// Load all of the saved posistions from non volatile memory
	static uint8_t read_buffer[512];
	const struct flash_area* switch_tracking_partition = NULL;
	int id = FIXED_PARTITION_ID(switch_tracking_partition);

	if(flash_area_open(id, &switch_tracking_partition) < 0){
		return;
	}

	int current_switch = 0;
	memset(read_buffer, 0, sizeof(read_buffer));
	flash_area_read(switch_tracking_partition, 0, &read_buffer, sizeof(read_buffer));
	for(int x = 0; x < 512; x++){
		for(int shift = 6; shift >= 0; shift -= 2){
			int val = read_buffer[x] & (0x3 << shift);
			val = val >> shift;

			if(val == 1){
				lcc_tortoise_state.trackers[current_switch].current_pos = SWITCH_NORMAL;
				printf("%d is normal\n", current_switch);
			}else if(val == 2){
				lcc_tortoise_state.trackers[current_switch].current_pos = SWITCH_REVERSED;
				printf("%d is rev\n", current_switch);
			}else{
				lcc_tortoise_state.trackers[current_switch].current_pos = SWITCH_UNKNOWN;
			}
			current_switch++;
		}
	}

	flash_area_close(switch_tracking_partition);
}

void switch_tracker_incoming_switch_command(uint16_t accy_number, enum dcc_accessory_direction accy_dir){
	int active = 0;
	int current = lcc_tortoise_state.trackers[accy_number].current_pos;

	if(accy_number > 2048){
		return;
	}

	if(accy_dir == ACCESSORY_NORMAL){
		active = 1;
		lcc_tortoise_state.trackers[accy_number].current_pos = SWITCH_NORMAL;
	}else if(accy_dir == ACCESSORY_REVERSE){
		lcc_tortoise_state.trackers[accy_number].current_pos = SWITCH_REVERSED;
	}else{
		return;
	}

	if(current == lcc_tortoise_state.trackers[accy_number].current_pos){
		// No change
		return;
	}

	printf("switch %d dir %d\n", accy_number, accy_dir);
	set_switch_tracker_dirty();
	struct lcc_accessory_address address;
	uint64_t event_id;
	struct lcc_event_context* evt_ctx = lcc_context_get_event_context(lcc_tortoise_state.lcc_context);

	address.active = active;
	address.dcc_accessory_address = accy_number;
	address.reserved = 0;

	if(lcc_accessory_decoder_to_event_id_2040(&address, &event_id) == LCC_OK){
		lcc_event_produce_event(evt_ctx, event_id);
	}
}
