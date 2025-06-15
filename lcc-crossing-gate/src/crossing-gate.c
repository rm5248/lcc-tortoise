/*
 * crossing-gate.c
 *
 *  Created on: Jun 9, 2025
 *      Author: robert
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "crossing-gate.h"

static void blink_gates(){
	k_sleep(K_MSEC(20));

	while(1){
		gpio_pin_toggle_dt(&crossing_gate_state.led[0]);
		gpio_pin_toggle_dt(&crossing_gate_state.led[1]);
		gpio_pin_toggle_dt(&crossing_gate_state.led[2]);
		k_sleep(K_MSEC(500));
	}
}

K_THREAD_DEFINE(gate_blink, 512, blink_gates, NULL, NULL, NULL,
		7, 0, 0);

static void handle_route_ltr(struct route* route, int left_input, int left_island_input, int right_island_input, int right_input){
	unsigned long time_since_last_change = k_uptime_get() - route->current_train.last_seen_millis;
	if(time_since_last_change < 500){
		// Debounce how fast we change state
		return;
	}

	if(left_island_input == 1 &&
			route->current_train.location == LOCATION_PRE_ISLAND_OCCUPIED){
		route->current_train.location = LOCATION_ISLAND_OCCUPIED_INCOMING;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("Island occupied incoming\n");
	}else if(right_island_input == 1 &&
			route->current_train.location == LOCATION_ISLAND_OCCUPIED_INCOMING){
		route->current_train.location = LOCATION_ISLAND_OCCUPIED;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("island occupied\n");
	}else if(right_island_input == 0 &&
			route->current_train.location == LOCATION_ISLAND_OCCUPIED){
		route->current_train.location = LOCATION_POST_ISLAND_OCCUPIED_INCOMING;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("post island occupied incoming\n");
	}else if(right_input == 1 &&
			route->current_train.location == LOCATION_POST_ISLAND_OCCUPIED_INCOMING){
		route->current_train.location = LOCATION_POST_ISLAND_OCCUPIED;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("post island occupied\n");
	}else if(right_input == 0 &&
			route->current_train.location == LOCATION_POST_ISLAND_OCCUPIED){
		printf("train out LTR\n");
		route->current_train.location = LOCATION_UNOCCUPIED;
		route->current_train.direction = DIRECTION_UNKNOWN;
		route->time_cleared_ms = k_uptime_get();
	}
}

static void handle_route_rtl(struct route* route, int left_input, int left_island_input, int right_island_input, int right_input){
	unsigned long time_since_last_change = k_uptime_get() - route->current_train.last_seen_millis;
	if(time_since_last_change < 500){
		// Debounce how fast we change state
		return;
	}

	if(right_island_input == 1 &&
			route->current_train.location == LOCATION_PRE_ISLAND_OCCUPIED){
		route->current_train.location = LOCATION_ISLAND_OCCUPIED_INCOMING;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("Island occupied incoming\n");
	}else if(left_island_input == 1 &&
			route->current_train.location == LOCATION_ISLAND_OCCUPIED_INCOMING){
		route->current_train.location = LOCATION_ISLAND_OCCUPIED;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("island occupied\n");
	}else if(left_island_input == 0 &&
			route->current_train.location == LOCATION_ISLAND_OCCUPIED){
		route->current_train.location = LOCATION_POST_ISLAND_OCCUPIED_INCOMING;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("post island occupied incoming\n");
	}else if(left_input == 1 &&
			route->current_train.location == LOCATION_POST_ISLAND_OCCUPIED_INCOMING){
		route->current_train.location = LOCATION_POST_ISLAND_OCCUPIED;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("post island occupied\n");
	}else if(left_input == 0 &&
			route->current_train.location == LOCATION_POST_ISLAND_OCCUPIED){
		printf("train out RTL\n");
		route->current_train.location = LOCATION_UNOCCUPIED;
		route->current_train.direction = DIRECTION_UNKNOWN;
		route->time_cleared_ms = k_uptime_get();
	}
}

static void crossing_gate_handle_single_route(struct route* route){
	for(int x = 0; x < ARRAY_SIZE(route->inputs); x++){
		if(!sensor_input_valid(&route->inputs[x])){
			return;
		}
	}

	int left_input = sensor_input_value(&route->inputs[0]);
	int left_island_input = sensor_input_value(&route->inputs[1]);
	int right_island_input = sensor_input_value(&route->inputs[2]);
	int right_input = sensor_input_value(&route->inputs[3]);
	unsigned long millis_diff = k_uptime_get() - route->current_train.last_seen_millis;
	unsigned long time_since_route_clear = k_uptime_get() - route->time_cleared_ms;

	// First check to see if this is a new train coming into the route
	if((left_input || right_input) &&
			route->current_train.location == LOCATION_UNOCCUPIED &&
			time_since_route_clear >= 2000){
		// There is a new train coming into the route.
		// Let's see if this route is valid or not
		for(int x = 0; x < sizeof(route->switch_inputs) / sizeof(route->switch_inputs[0]); x++){
			if(switch_input_value(&route->switch_inputs[x]) != route->switch_inputs[x].route_position){
				// The switch is not set to the right position for this route to be active
				return;
			}
		}

		// All switches are in the correct position for this route.
		// This route now has a train in it
		route->current_train.last_seen_millis = k_uptime_get();
		route->current_train.location = LOCATION_PRE_ISLAND_OCCUPIED;
		if(left_input){
			route->current_train.direction = DIRECTION_LTR;
			printf("Incoming train LTR\n");
		}else{
			route->current_train.direction = DIRECTION_RTL;
			printf("Incoming train RTL\n");
		}

		return;
	}

	if(route->current_train.direction == DIRECTION_RTL){
		handle_route_rtl(route, left_input, left_island_input, right_island_input, right_input);
	}else if(route->current_train.direction == DIRECTION_LTR){
		handle_route_ltr(route, left_input, left_island_input, right_island_input, right_input);
	}

	// Serial.print("millis diff: ");
	// Serial.println(millis_diff);
	if((millis_diff > crossing_gate_state.timeout_millis) &&
			route->current_train.location != LOCATION_UNOCCUPIED){
		printf("timeout\n");
		route->current_train.location = LOCATION_UNOCCUPIED;
		route->current_train.direction = DIRECTION_UNKNOWN;
		route->time_cleared_ms = k_uptime_get();
	}
}

/**
 * Determine if a route is occupied for a flash state.
 * The route is occupied if:
 *  - The pre-island is occupied
 *  - the island is occupied
 *
 * Once the train passes the island, it is no longer considered occupied for flashing purposes
 */
int is_route_occupied_for_flash(struct route* route){
	if(route->current_train.location == LOCATION_PRE_ISLAND_OCCUPIED ||
		route->current_train.location == LOCATION_ISLAND_OCCUPIED_INCOMING ||
		route->current_train.location == LOCATION_ISLAND_OCCUPIED){
		return 1;
	}

	return 0;
}

static void crossing_gate_flash(){
	enum GateFlashState expectedGateFlashState = FLASH_OFF;

	for(int x = 0; x < ARRAY_SIZE(crossing_gate_state.crossing_routes); x++){
		if(is_route_occupied_for_flash(&crossing_gate_state.crossing_routes[x])){
			expectedGateFlashState = FLASH_ON;
		}
	}

	if(expectedGateFlashState != crossing_gate_state.gate_flash_state){
		crossing_gate_state.gate_flash_state = expectedGateFlashState;

		if(crossing_gate_state.gate_flash_state == FLASH_ON){
			printf("Flash on\n");
			// Lower the gates
			gpio_pin_set_dt(&crossing_gate_state.led[0], 0);
			gpio_pin_set_dt(&crossing_gate_state.led[1], 1);
			gpio_pin_set_dt(&crossing_gate_state.led[2], 0);
			k_thread_resume(gate_blink);
		}
	}
}

void crossing_gate_update(){
	for(int x = 0; x < ARRAY_SIZE(crossing_gate_state.crossing_routes); x++){
		crossing_gate_handle_single_route(&crossing_gate_state.crossing_routes[x]);
	}

	crossing_gate_flash();
}

void crossing_gate_incoming_event(uint64_t event_id){

}
