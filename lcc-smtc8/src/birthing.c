/*
 * birthing.c
 *
 *  Created on: Jan 5, 2025
 *      Author: robert
 */

#include <zephyr/drivers/gpio.h>

#include "lcc-tortoise-state.h"
#include "birthing.h"
#include "tortoise.h"

static int init_led(const struct gpio_dt_spec* led){
	if (!gpio_is_ready_dt(led)) {
		return -1;
	}

	if(gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE) < 0){
		return -1;
	}

	return 0;
}

static int init_button(const struct gpio_dt_spec* button){
	if (!gpio_is_ready_dt(button)) {
		return -1;
	}

	if(gpio_pin_configure_dt(button, GPIO_INPUT) < 0){
		return -1;
	}

	if(gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_BOTH) < 0){
		return -1;
	}

	return 0;
}

static void panic(){
	printf("Unable to init GPIO!\n");
	while(1){

	}
}

static void do_tortoise_state(int state){
	for(int x = 0; x < 8; x++){
		struct tortoise* tort = &lcc_tortoise_state.tortoises[x];

		switch(state){
		case 0:
			gpio_pin_set_dt(&tort->gpios[0], 0);
			gpio_pin_set_dt(&tort->gpios[1], 0);
			break;
		case 1:
			gpio_pin_set_dt(&tort->gpios[0], 1);
			gpio_pin_set_dt(&tort->gpios[1], 0);
			break;
		case 2:
			gpio_pin_set_dt(&tort->gpios[0], 0);
			gpio_pin_set_dt(&tort->gpios[1], 1);
			break;
		case 3:
			gpio_pin_set_dt(&tort->gpios[0], 1);
			gpio_pin_set_dt(&tort->gpios[1], 1);
			break;
		}
	}
}

void smtc8_birthing(){
	if(init_led(&lcc_tortoise_state.green_led) < 0){
		panic();
	}
	if(init_led(&lcc_tortoise_state.gold_led) < 0){
		panic();
	}
	if(init_led(&lcc_tortoise_state.blue_led) < 0){
		panic();
	}
	if(init_button(&lcc_tortoise_state.blue_button) < 0){
		panic();
	}
	if(init_button(&lcc_tortoise_state.gold_button) < 0){
		panic();
	}

	for(int x = 0; x < 8; x++){
		int ret;
		struct tortoise* tort = &lcc_tortoise_state.tortoises[x];
		ret = gpio_pin_configure_dt(&tort->gpios[0], GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			panic();
		}
		ret = gpio_pin_configure_dt(&tort->gpios[1], GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			panic();
		}
	}

	printf("Press blue button to go forward in state, gold to exit\n");
	int state = 0;
	int blue_state = 0;
	int gold_state = 0;
	while(1){
		int new_blue_state = gpio_pin_get_dt(&lcc_tortoise_state.blue_button);
		int new_gold_state = gpio_pin_get_dt(&lcc_tortoise_state.gold_button);
		if(new_blue_state == 1 && blue_state == 0){
			state++;
			if(state > 3){
				state = 0;
			}
			printf("State: %d\n", state);
			gpio_pin_toggle_dt(&lcc_tortoise_state.blue_led);
			gpio_pin_toggle_dt(&lcc_tortoise_state.gold_led);
			gpio_pin_toggle_dt(&lcc_tortoise_state.green_led);
		}
		if(new_gold_state == 1 && gold_state == 0){
			break;
		}
		blue_state = new_blue_state;
		do_tortoise_state(state);
	}
}
