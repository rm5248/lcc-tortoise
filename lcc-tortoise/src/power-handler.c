/*
 * power-handler.c
 *
 *  Created on: Aug 11, 2024
 *      Author: robert
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"

const static struct gpio_dt_spec volts_line = GPIO_DT_SPEC_GET(DT_NODELABEL(volts), gpios);
static struct gpio_callback volts_cb_data;

static void power_lost_irq(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	for(int x = 0; x < 8; x++){
		tortoise_disable_outputs(&lcc_tortoise_state.tortoises[x]);
	}
	while(1){
		printf(".");
		k_msleep(1);
	}
}

void powerhandle_init(){
	int ret;
	if (!gpio_is_ready_dt(&volts_line)) {
		printk("Error: volts not ready\n");
		return;
	}

	ret = gpio_pin_configure_dt(&volts_line, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure volts line\n");
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&volts_line,
						  GPIO_INT_TRIG_LOW);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on volts\n");
		return;
	}

	gpio_init_callback(&volts_cb_data, power_lost_irq, BIT(volts_line.pin));
	gpio_add_callback(volts_line.port, &volts_cb_data);
}
