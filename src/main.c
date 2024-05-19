/*
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000


const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static struct gpio_callback blue_button_cb_data;
static struct gpio_callback gold_button_cb_data;

int main(void)
{
	int ret;
	bool led_state = true;

	if(lcc_tortoise_state_init() < 0){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize!\n");
		return 0;
	}

	// TODO: what state should the tortoise be in on boot? Need to query from non-volatile memory
	// For now, let's just set them all to normal
	for(int x = 0; x < 8; x++){
		tortoise_set_position(&lcc_tortoise_state.tortoises[x], NORMAL);
	}

	if (!device_is_ready(can_dev)) {
		printf("CAN: Device %s not ready.\n", can_dev->name);
		return 0;
	}

	ret = can_start(can_dev);

	int x = 0;
	int pos = 0;
	while (1) {
		ret = gpio_pin_toggle_dt(&lcc_tortoise_state.green_led);
		if (ret < 0) {
			return 0;
		}

		led_state = !led_state;
		printf("LED state: %s\n", led_state ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
