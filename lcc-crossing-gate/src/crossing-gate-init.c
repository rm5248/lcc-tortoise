/*
 * crossing-gate-init.c
 *
 *  Created on: Jun 14, 2025
 *      Author: robert
 */

#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/storage/flash_map.h>

#include "crossing-gate-init.h"
#include "crossing-gate.h"
#include "partition_utils.h"

// The global singleton instance
struct crossing_gate crossing_gate_state = {
		.gate_flash_state = FLASH_OFF,
		.timeout_millis = 25000,
		.blue_button = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_switch), gpios),
		.gold_button = GPIO_DT_SPEC_GET(DT_NODELABEL(gold_switch), gpios),
		.config_mode = CONFIG_MODE_NORMAL,
//		.crossing_routes = {0},
		.inputs = {
				GPIO_DT_SPEC_GET(DT_NODELABEL(in0), gpios),
				GPIO_DT_SPEC_GET(DT_NODELABEL(in1), gpios),
				GPIO_DT_SPEC_GET(DT_NODELABEL(in2), gpios),
				GPIO_DT_SPEC_GET(DT_NODELABEL(in3), gpios),
				GPIO_DT_SPEC_GET(DT_NODELABEL(in4), gpios),
				GPIO_DT_SPEC_GET(DT_NODELABEL(in5), gpios),
				GPIO_DT_SPEC_GET(DT_NODELABEL(in6), gpios),
				GPIO_DT_SPEC_GET(DT_NODELABEL(in7), gpios),
		},
		.tortoise_control = {
				{
					.gpios = {
							GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 1),
							GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 0)
					}
				},
				{
						.gpios = {
							GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise1), gpios, 1),
							GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise1), gpios, 0)
					},
				},
		},
		.bell = {
				.enable = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), sound_gpios),
				.time_millis = 0,
				.ring_type = BELL_RING_NONE,
		},
		.led = {
//				GPIO_DT_SPEC_GET(DT_NODELABEL(led_out1), gpios),
//				GPIO_DT_SPEC_GET(DT_NODELABEL(led_out2), gpios),
//				GPIO_DT_SPEC_GET(DT_NODELABEL(led_out3), gpios),
		},
		.led_pwm = DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(pwm_leds)),
		.tortoise_power = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), tortoise_power_gpios),
};

static struct gpio_callback cb_data[8];

static void input_change(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	int real_pin = -1;

	// Map between the bitmask and our pin input
	for(int bit_num = 0; bit_num < 8; bit_num++){
		if(pins == BIT(bit_num)){
			real_pin = bit_num;
		}
	}

	if(real_pin < 0){
		return;
	}

	k_msgq_put(&crossing_gate_state.pin_change_msgq, &real_pin, K_NO_WAIT);
}

static void init_button(const struct gpio_dt_spec* button){
	if (!gpio_is_ready_dt(button)) {
		return;
	}
	gpio_pin_configure_dt(button, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_BOTH);
}

static void init_input(const struct gpio_dt_spec* in, struct gpio_callback* cb_data){
	if (!gpio_is_ready_dt(in)) {
		return;
	}

	if(gpio_pin_configure_dt(in, GPIO_INPUT) < 0){
		return;
	}

	if(gpio_pin_interrupt_configure_dt(in, GPIO_INT_EDGE_BOTH) < 0){
		return;
	}

	gpio_init_callback(cb_data, input_change, BIT(in->pin));
	gpio_add_callback(in->port, cb_data);
}

static void init_tortoise(const struct tortoise* tort){
	if(!gpio_is_ready_dt(&tort->gpios[0])){
		return;
	}
	if(!gpio_is_ready_dt(&tort->gpios[1])){
		return;
	}

	if(gpio_pin_configure_dt(&tort->gpios[0], GPIO_OUTPUT) < 0){
		return;
	}
	if(gpio_pin_configure_dt(&tort->gpios[1], GPIO_OUTPUT) < 0){
		return;
	}
}

static void init_single_route(int route_num){
	struct route* current_route = &crossing_gate_state.crossing_routes[route_num];
	struct route_config* route_config = &crossing_gate_state.routes_config.all_routes[route_num];
	current_route->config = route_config;

	for(int sensor = 0; sensor < ARRAY_SIZE(current_route->sensors); sensor++){
		struct sensor_input* input = &current_route->sensors[sensor];
		input->config = &route_config->inputs[sensor];
		sensor_input_init(input, &crossing_gate_state.inputs[input->config->sensor_input]);
	}

	for(int turnout = 0; turnout < ARRAY_SIZE(current_route->switch_inputs); turnout++){
		struct switch_input* input = &current_route->switch_inputs[turnout];
		input->config = &route_config->switch_inputs[turnout];
		switch_input_init(input, &crossing_gate_state.inputs[input->config->swithc_input]);
	}
}

void crossing_gate_load_config(){
	partition_util_load(FIXED_PARTITION_ID(segment_253),
			&crossing_gate_state.routes_config,
			sizeof(crossing_gate_state.routes_config));
	partition_util_load(FIXED_PARTITION_ID(segment_252),
			&crossing_gate_state.general_events,
			sizeof(crossing_gate_state.general_events));
	partition_util_load(FIXED_PARTITION_ID(segment_251),
			&crossing_gate_state.node_info,
			sizeof(crossing_gate_state.node_info));
	partition_util_load(FIXED_PARTITION_ID(segment_250),
			&crossing_gate_state.general_config,
			sizeof(crossing_gate_state.general_config));
	partition_util_load(FIXED_PARTITION_ID(segment_249),
			&crossing_gate_state.pwm_config,
			sizeof(crossing_gate_state.pwm_config));

	// Now that we have loaded the config, we need to update our data structures in RAM so
	// that they are pointed at the correct things
	for(int route_num = 0; route_num < ARRAY_SIZE(crossing_gate_state.crossing_routes); route_num++){
		init_single_route(route_num);
	}
}

void crossing_gate_set_default_values(uint64_t base_event_id){
	// Segment 250
	crossing_gate_state.general_config.timeout = 25;
	crossing_gate_state.general_config.bell_behavior = 4;
	crossing_gate_state.general_config.bell_ring_time = 0;

	// Segment 251
	memset(crossing_gate_state.node_info.description, 0, sizeof(crossing_gate_state.node_info.description));
	memset(crossing_gate_state.node_info.name, 0, sizeof(crossing_gate_state.node_info.name));

	// Segment 252
	crossing_gate_state.general_events.BE_gates_active_event = __builtin_bswap64(base_event_id++);
	crossing_gate_state.general_events.BE_gates_inactive_event = __builtin_bswap64(base_event_id++);
	crossing_gate_state.general_events.BE_manual_gates_activate_event = __builtin_bswap64(base_event_id++);
	for(int x = 0; x < ARRAY_SIZE(crossing_gate_state.general_events.inputs); x++){
		memset(crossing_gate_state.general_events.inputs[x].input_name, 0, sizeof(crossing_gate_state.general_events.inputs[x].input_name));
		crossing_gate_state.general_events.inputs[x].BE_input_activated_event = __builtin_bswap64(base_event_id++);
		crossing_gate_state.general_events.inputs[x].BE_input_deactivated_event = __builtin_bswap64(base_event_id++);
	}

	// Segment 253
	for(int x = 0; x < ARRAY_SIZE(crossing_gate_state.routes_config.all_routes); x++){
		struct route_config* cfg = &crossing_gate_state.routes_config.all_routes[x];
		memset(cfg->route_name, 0, sizeof(cfg->route_name));

		for(int y = 0; y < ARRAY_SIZE(cfg->inputs); y++){
			memset(&cfg->inputs[y], 0, sizeof(cfg->inputs[y]));
			cfg->inputs[y].BE_event_sensor_on = __builtin_bswap64(base_event_id++);
			cfg->inputs[y].BE_event_sensor_off = __builtin_bswap64(base_event_id++);
		}

		for(int z = 0; z < ARRAY_SIZE(cfg->switch_inputs); z++){
			memset(&cfg->switch_inputs[z], 0, sizeof(cfg->switch_inputs[z]));
			cfg->switch_inputs[z].BE_event_switch_normal = __builtin_bswap64(base_event_id++);
			cfg->switch_inputs[z].BE_event_switch_reversed = __builtin_bswap64(base_event_id++);
		}
	}

	// Segment 249
	memset(&crossing_gate_state.pwm_config, 0, sizeof(struct pwm_output_segment));

	crossing_gate_state.general_config.base_event_id = base_event_id;

	// Now that we have initialized everything to sane values, set up two routes
	// Route 1 uses inputs 1-4, and Route 2 uses inputs 5-8
	struct route* route1 = &crossing_gate_state.crossing_routes[0];
	route1->config->route_enabled = 1;
	strcpy(route1->config->route_name, "Track 1");
	route1->sensors[0].config->sensor_enabled = 1;
	route1->sensors[0].config->sensor_input = 0;
	route1->sensors[1].config->sensor_enabled = 1;
	route1->sensors[1].config->sensor_input = 1;
	route1->sensors[2].config->sensor_enabled = 1;
	route1->sensors[2].config->sensor_input = 2;
	route1->sensors[3].config->sensor_enabled = 1;
	route1->sensors[3].config->sensor_input = 3;

	struct route* route2 = &crossing_gate_state.crossing_routes[1];
	route2->config->route_enabled = 1;
	strcpy(route2->config->route_name, "Track 2");
	route2->sensors[0].config->sensor_enabled = 1;
	route2->sensors[0].config->sensor_input = 4;
	route2->sensors[1].config->sensor_enabled = 1;
	route2->sensors[1].config->sensor_input = 5;
	route2->sensors[2].config->sensor_enabled = 1;
	route2->sensors[2].config->sensor_input = 6;
	route2->sensors[3].config->sensor_enabled = 1;
	route2->sensors[3].config->sensor_input = 7;
}

void crossing_gate_init(){
	k_msgq_init(&crossing_gate_state.pin_change_msgq, (char*)crossing_gate_state.pin_changemsgq_buffer, sizeof(int), 10);

	led_set_brightness(crossing_gate_state.led_pwm, 0, 0);
	led_set_brightness(crossing_gate_state.led_pwm, 1, 0);

	memset(crossing_gate_state.crossing_routes, 0, sizeof(crossing_gate_state.crossing_routes));
	for(int x = 0; x < ARRAY_SIZE(crossing_gate_state.crossing_routes); x++){
		k_timer_init(&crossing_gate_state.crossing_routes[x].timeout, crossing_gate_timer_expired, NULL);
		k_timer_init(&crossing_gate_state.crossing_routes[x].reactivation_timeout, NULL, NULL);
		crossing_gate_state.crossing_routes[x].config = &(crossing_gate_state.routes_config.all_routes[x]);
	}

	for(int x = 0; x < ARRAY_SIZE(crossing_gate_state.inputs); x++){
		init_input(&crossing_gate_state.inputs[x], &cb_data[x]);
	}

	init_button(&crossing_gate_state.blue_button);
	init_button(&crossing_gate_state.gold_button);

	for(int x = 0; x < 2; x++){
		init_tortoise(&crossing_gate_state.tortoise_control[x]);
	}

	// Enable the power supply for the tortoises
	gpio_pin_configure_dt(&crossing_gate_state.tortoise_power, GPIO_OUTPUT);
	gpio_pin_set_dt(&crossing_gate_state.tortoise_power, 1);

	crossing_gate_raise_arms();

	gpio_pin_configure_dt(&crossing_gate_state.bell.enable, GPIO_OUTPUT);

	// Configure our PWM LED outputs
//	gpio_pin_configure_dt(&crossing_gate_state.led[0], GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);
//	gpio_pin_configure_dt(&crossing_gate_state.led[1], GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);
//	gpio_pin_configure_dt(&crossing_gate_state.led[2], GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);

	// statically configure a route for testing purposes
//	crossing_gate_state.crossing_routes[0].sensors[0].sensor_gpio = &crossing_gate_state.inputs[0];
//	crossing_gate_state.crossing_routes[0].sensors[1].sensor_gpio = &crossing_gate_state.inputs[1];
//	crossing_gate_state.crossing_routes[0].sensors[2].sensor_gpio = &crossing_gate_state.inputs[2];
//	crossing_gate_state.crossing_routes[0].sensors[3].sensor_gpio = &crossing_gate_state.inputs[3];
//	strcpy(crossing_gate_state.crossing_routes[0].config->route_name, "Route 1");
}

static void init_single_pwm(int idx){
	int zero_value = 0;
	if(crossing_gate_state.pwm_config.pwm_configs[idx].polarity == 1){
		zero_value = 100;
	}

	led_set_brightness(crossing_gate_state.led_pwm, idx, zero_value);
}

void crossing_gate_do_pwm_config(){
	for(int x = 0; x < 6; x++){
		init_single_pwm(x);
	}
}
