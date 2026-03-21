/*
 * crossing-gate.c
 *
 *  Created on: Jun 9, 2025
 *      Author: robert
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/pwm.h>

#include "crossing-gate.h"

static void blink_gates(){
	k_sleep(K_MSEC(20));
	int led1_brightness = 0;
	int led2_brightness = 0;
	int dir = 0;

	while(1){
		if(crossing_gate_state.gate_flash_state == FLASH_OFF &&
						led1_brightness == 0 &&
						led2_brightness == 0){
					k_sleep(K_MSEC(100));
					dir = 1;
					continue;
		}else if(crossing_gate_state.gate_flash_state == FLASH_OFF){
			// Ramp down until everything is off
			if(led1_brightness){
				led_set_brightness(crossing_gate_state.led_pwm, 0, led1_brightness);
				led1_brightness--;
			}
			if(led2_brightness){
				led_set_brightness(crossing_gate_state.led_pwm, 1, led2_brightness);
				led2_brightness--;
			}
			k_sleep(K_MSEC(2));
		}else{
			led_set_brightness(crossing_gate_state.led_pwm, 0, led1_brightness);
			led_set_brightness(crossing_gate_state.led_pwm, 1, led2_brightness);

			if(dir){
				led1_brightness++;
				led2_brightness--;
				if(led1_brightness >= 100){
					dir = 0;
				}
				if(led2_brightness < 0){
					led2_brightness = 0;
				}
			}else{
				led1_brightness--;
				led2_brightness++;
				if(led1_brightness <= 0){
					dir = 1;
				}
				if(led2_brightness > 100){
					led2_brightness = 100;
				}
			}
		}

		k_sleep(K_MSEC(2));
		if(crossing_gate_state.gate_flash_state == FLASH_ON &&
				(led1_brightness == 0 || led1_brightness == 100)){
			k_sleep(K_MSEC(250));
		}
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
		printf("Route %s: Island occupied incoming\n", route->config->route_name);
	}else if(right_island_input == 1 &&
			route->current_train.location == LOCATION_ISLAND_OCCUPIED_INCOMING){
		route->current_train.location = LOCATION_ISLAND_OCCUPIED;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("Route %s: island occupied\n", route->config->route_name);
	}else if(right_island_input == 0 &&
			route->current_train.location == LOCATION_ISLAND_OCCUPIED){
		route->current_train.location = LOCATION_POST_ISLAND_OCCUPIED_INCOMING;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("Route %s: post island occupied incoming\n", route->config->route_name);
	}else if(right_input == 1 &&
			route->current_train.location == LOCATION_POST_ISLAND_OCCUPIED_INCOMING){
		route->current_train.location = LOCATION_POST_ISLAND_OCCUPIED;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("Route %s: post island occupied\n", route->config->route_name);
	}else if(right_input == 0 &&
			route->current_train.location == LOCATION_POST_ISLAND_OCCUPIED){
		printf("Route %s: train out LTR\n", route->config->route_name);
		route->current_train.location = LOCATION_UNOCCUPIED;
		route->current_train.direction = DIRECTION_UNKNOWN;
//		route->time_cleared_ms = k_uptime_get();
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
		printf("Route %s: Island occupied incoming\n", route->config->route_name);
	}else if(left_island_input == 1 &&
			route->current_train.location == LOCATION_ISLAND_OCCUPIED_INCOMING){
		route->current_train.location = LOCATION_ISLAND_OCCUPIED;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("Route %s: island occupied\n", route->config->route_name);
	}else if(left_island_input == 0 &&
			route->current_train.location == LOCATION_ISLAND_OCCUPIED){
		route->current_train.location = LOCATION_POST_ISLAND_OCCUPIED_INCOMING;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("Route %s: post island occupied incoming\n", route->config->route_name);
	}else if(left_input == 1 &&
			route->current_train.location == LOCATION_POST_ISLAND_OCCUPIED_INCOMING){
		route->current_train.location = LOCATION_POST_ISLAND_OCCUPIED;
		route->current_train.last_seen_millis = k_uptime_get();
		printf("Route %s: post island occupied\n", route->config->route_name);
	}else if(left_input == 0 &&
			route->current_train.location == LOCATION_POST_ISLAND_OCCUPIED){
		printf("Route %s: train out RTL\n", route->config->route_name);
		route->current_train.location = LOCATION_UNOCCUPIED;
		route->current_train.direction = DIRECTION_UNKNOWN;
//		route->time_cleared_ms = k_uptime_get();
	}
}

static void crossing_gate_handle_single_route(struct route* route){
	for(int x = 0; x < ARRAY_SIZE(route->sensors); x++){
		if(!sensor_input_valid(&route->sensors[x])){
			return;
		}
	}

	int left_input = sensor_input_value(&route->sensors[0]);
	int left_island_input = sensor_input_value(&route->sensors[1]);
	int right_island_input = sensor_input_value(&route->sensors[2]);
	int right_input = sensor_input_value(&route->sensors[3]);
//	unsigned long millis_diff = k_uptime_get() - route->current_train.last_seen_millis;
//	unsigned long time_since_route_clear = k_uptime_get() - route->time_cleared_ms;

	printf("Route %s: left: %d left island: %d right island: %d right: %d\n",
			route->config->route_name,
			left_input,
			left_island_input,
			right_island_input,
			right_input);

	// First check to see if this is a new train coming into the route
	if((left_input || right_input) &&
			route->current_train.location == LOCATION_UNOCCUPIED){
		// There is a new train coming into the route.
		// Let's see if this route is valid or not
		for(int x = 0; x < sizeof(route->switch_inputs) / sizeof(route->switch_inputs[0]); x++){
			if(switch_input_value(&route->switch_inputs[x]) != route->config->switch_inputs[x].polarity){
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
			printf("Route %s: Incoming train LTR\n", route->config->route_name);
		}else{
			route->current_train.direction = DIRECTION_RTL;
			printf("Route %s: Incoming train RTL\n", route->config->route_name);
		}

		k_timer_start(&route->timeout, K_MSEC(15000), K_NO_WAIT);
		return;
	}

	if(route->current_train.direction == DIRECTION_RTL){
		handle_route_rtl(route, left_input, left_island_input, right_island_input, right_input);
	}else if(route->current_train.direction == DIRECTION_LTR){
		handle_route_ltr(route, left_input, left_island_input, right_island_input, right_input);
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
			crossing_gate_lower_arms();
		}else if(crossing_gate_state.gate_flash_state == FLASH_OFF){
			crossing_gate_raise_arms();
			led_set_brightness(crossing_gate_state.led_pwm, 0, 0);
			led_set_brightness(crossing_gate_state.led_pwm, 1, 0);
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

void crossing_gate_raise_arms(){
	gpio_pin_set_dt(&crossing_gate_state.tortoise_control[0].gpios[0], 0);
	gpio_pin_set_dt(&crossing_gate_state.tortoise_control[0].gpios[1], 1);
	gpio_pin_set_dt(&crossing_gate_state.tortoise_control[1].gpios[0], 0);
	gpio_pin_set_dt(&crossing_gate_state.tortoise_control[1].gpios[1], 1);

	gpio_pin_set_dt(&crossing_gate_state.bell.enable, 0);
}

void crossing_gate_lower_arms(){
	gpio_pin_set_dt(&crossing_gate_state.tortoise_control[0].gpios[0], 1);
	gpio_pin_set_dt(&crossing_gate_state.tortoise_control[0].gpios[1], 0);
	gpio_pin_set_dt(&crossing_gate_state.tortoise_control[1].gpios[0], 1);
	gpio_pin_set_dt(&crossing_gate_state.tortoise_control[1].gpios[1], 0);

	gpio_pin_set_dt(&crossing_gate_state.bell.enable, 1);
}

void crossing_gate_timer_expired(struct k_timer* timer_id){
	struct route* route = NULL;

	for(int x = 0; x < ARRAY_SIZE(crossing_gate_state.crossing_routes); x++){
		if(&(crossing_gate_state.crossing_routes[x].timeout) == timer_id){
			route = &crossing_gate_state.crossing_routes[x];
		}
	}

	if(route == NULL){
		printf("Unable to find route for timer??\n");
		return;
	}

	k_timer_stop(timer_id);

	if(route->current_train.location != LOCATION_UNOCCUPIED){
		printf("Route %s: timeout\n", route->config->route_name);
		route->current_train.location = LOCATION_UNOCCUPIED;
		route->current_train.direction = DIRECTION_UNKNOWN;

		char data = 0;
		k_msgq_put(&crossing_gate_state.process_msgq, &data, K_NO_WAIT);
	}
}
