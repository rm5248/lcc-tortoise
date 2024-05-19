/*
 * tortoise.c
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "tortoise.h"

int tortoise_init(struct tortoise* tort){
	int ret = 0;

	if (!gpio_is_ready_dt(&tort->gpios[0])) {
		return -1;
	}
	if (!gpio_is_ready_dt(&tort->gpios[1])) {
		return -1;
	}

	ret = gpio_pin_configure_dt(&tort->gpios[0], GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&tort->gpios[1], GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return -1;
	}

	return 0;
}

int tortoise_set_position(struct tortoise* tort, enum tortoise_position position){
	gpio_pin_set_dt(&tort->gpios[0], 0);
	gpio_pin_set_dt(&tort->gpios[1], 0);

	if(position == NORMAL){
		gpio_pin_set_dt(&tort->gpios[0], 0);
		gpio_pin_set_dt(&tort->gpios[1], 1);
	}else{
		gpio_pin_set_dt(&tort->gpios[0], 1);
		gpio_pin_set_dt(&tort->gpios[1], 0);
	}

	return 0;
}
