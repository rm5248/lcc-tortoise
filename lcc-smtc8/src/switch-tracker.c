/*
 * switch-tracker.c
 *
 *  Created on: Dec 16, 2024
 *      Author: robert
 */
#include "switch-tracker.h"
#include "lcc-tortoise-state.h"

#include "lcc.h"
#include "lcc-common.h"
#include "lcc-event.h"

enum switch_position{
	SWITCH_UNKNOWN,
	SWITCH_NORMAL,
	SWITCH_REVERSED,
};

struct switch_tracker{
	enum switch_position current_pos;
};

static struct switch_tracker trackers[2048];

static enum lcc_producer_state switch_producer_state(struct lcc_context* ctx, uint64_t event_id){
	struct lcc_accessory_address addr = {0};
	enum lcc_producer_state ret = LCC_PRODUCER_UNKNOWN;

	if( lcc_event_id_to_accessory_decoder_2040(event_id, &addr) < 0){
		goto out;
	}

	if(addr.dcc_accessory_address < 0 || addr.dcc_accessory_address > 2048){
		goto out;
	}

	switch(trackers[addr.dcc_accessory_address].current_pos){
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
	memset(trackers, 0, sizeof(trackers));

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
}

void switch_tracker_incoming_switch_command(uint16_t accy_number, enum dcc_accessory_direction accy_dir){
	int active = 0;
	int current = trackers[accy_number].current_pos;

	if(accy_number > 2048){
		return;
	}

	if(accy_dir == ACCESSORY_NORMAL){
		active = 1;
		trackers[accy_number].current_pos = SWITCH_NORMAL;
	}else if(accy_dir == ACCESSORY_REVERSE){
		trackers[accy_number].current_pos = SWITCH_REVERSED;
	}else{
		return;
	}

	if(current == trackers[accy_number].current_pos){
		// No change
		return;
	}

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
