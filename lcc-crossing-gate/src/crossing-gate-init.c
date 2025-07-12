/*
 * crossing-gate-init.c
 *
 *  Created on: Jun 14, 2025
 *      Author: robert
 */

#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>

#include "crossing-gate-init.h"
#include "crossing-gate.h"

// The global singleton instance
struct crossing_gate crossing_gate_state = {
		.gate_flash_state = FLASH_OFF,
		.timeout_millis = 25000,
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
				.enable = GPIO_DT_SPEC_GET(DT_NODELABEL(sound_enable), gpios),
				.power = GPIO_DT_SPEC_GET(DT_NODELABEL(sound_power), gpios),
				.time_millis = 0,
				.ring_type = BELL_RING_NONE,
		},
		.led = {
//				GPIO_DT_SPEC_GET(DT_NODELABEL(led_out1), gpios),
//				GPIO_DT_SPEC_GET(DT_NODELABEL(led_out2), gpios),
//				GPIO_DT_SPEC_GET(DT_NODELABEL(led_out3), gpios),
		},
};

static struct gpio_callback cb_data[8];

static void input_change(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printf("input change\n");
	crossing_gate_update();
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

void crossing_gate_init(){
	k_thread_suspend(gate_blink);

	memset(crossing_gate_state.crossing_routes, 0, sizeof(crossing_gate_state.crossing_routes));

	for(int x = 0; x < 8; x++){
		init_input(&crossing_gate_state.inputs[x], &cb_data[x]);
	}

	for(int x = 0; x < 2; x++){
		init_tortoise(&crossing_gate_state.tortoise_control[x]);
	}
	crossing_gate_raise_arms();

	// Now let's enable the bell and the power
	gpio_pin_configure_dt(&crossing_gate_state.bell.enable, GPIO_OUTPUT);
	gpio_pin_configure_dt(&crossing_gate_state.bell.power, GPIO_OUTPUT);

	// Configure our PWM LED outputs
//	gpio_pin_configure_dt(&crossing_gate_state.led[0], GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);
//	gpio_pin_configure_dt(&crossing_gate_state.led[1], GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);
//	gpio_pin_configure_dt(&crossing_gate_state.led[2], GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);

	// statically configure a route for testing purposes
	crossing_gate_state.crossing_routes[0].inputs[0].sensor_gpio = &crossing_gate_state.inputs[0];
	crossing_gate_state.crossing_routes[0].inputs[1].sensor_gpio = &crossing_gate_state.inputs[1];
	crossing_gate_state.crossing_routes[0].inputs[2].sensor_gpio = &crossing_gate_state.inputs[2];
	crossing_gate_state.crossing_routes[0].inputs[3].sensor_gpio = &crossing_gate_state.inputs[3];
}
