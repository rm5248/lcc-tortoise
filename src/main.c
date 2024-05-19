/*
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000


static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(DT_ALIAS(greenled), gpios);
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(blueled), gpios);
static const struct gpio_dt_spec gold_led = GPIO_DT_SPEC_GET(DT_ALIAS(goldled), gpios);
static const struct gpio_dt_spec blue_button = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_switch), gpios);
static const struct gpio_dt_spec gold_button = GPIO_DT_SPEC_GET(DT_NODELABEL(gold_switch), gpios);
//static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(blueswitch), gpios);

//#if !DT_NODE_HAS_STATUS(DT_NODELABEL(tortoise0))
//#error "bad status"
//#endif

static const struct gpio_dt_spec tortoise0[] ={
		GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 0),
		GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 1)
};

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static struct gpio_callback blue_button_cb_data;
static struct gpio_callback gold_button_cb_data;

static int init_tortoise(const struct gpio_dt_spec* tortoise){
	int ret;

	if (!gpio_is_ready_dt(&tortoise[0])) {
		return 0;
	}
	if (!gpio_is_ready_dt(&tortoise[1])) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&tortoise[0], GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return 0;
	}
	ret = gpio_pin_configure_dt(&tortoise[1], GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return 0;
	}

	return 1;
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printf("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	printf("  pin: %d\n", pins);

	int ret;

	if(pins == BIT(blue_button.pin)){
		ret = gpio_pin_toggle_dt(&blue_led);
	}else if(pins == BIT(gold_button.pin)){
		ret = gpio_pin_toggle_dt(&gold_led);
	}

}

int main(void)
{
	int ret;
	bool led_state = true;

	if (!gpio_is_ready_dt(&green_led)) {
		return 0;
	}
	if(!gpio_is_ready_dt(&blue_led)){
		return 0;
	}


	ret = gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE);
	ret = gpio_pin_configure_dt(&gold_led, GPIO_OUTPUT_ACTIVE);

	if (!gpio_is_ready_dt(&blue_button)) {
		printk("Error: button device %s is not ready\n",
			   blue_button.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&blue_button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
			   ret, blue_button.port->name, blue_button.pin);
		return 0;
	}
	ret = gpio_pin_configure_dt(&gold_button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
			   ret, gold_button.port->name, gold_button.pin);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&blue_button,
						      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, blue_button.port->name, blue_button.pin);
		return 0;
	}
	ret = gpio_pin_interrupt_configure_dt(&gold_button,
						      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, gold_button.port->name, gold_button.pin);
		return 0;
	}

	gpio_init_callback(&blue_button_cb_data, button_pressed, BIT(blue_button.pin));
	gpio_add_callback(blue_button.port, &blue_button_cb_data);
	gpio_init_callback(&gold_button_cb_data, button_pressed, BIT(gold_button.pin));
	gpio_add_callback(gold_button.port, &gold_button_cb_data);

	if(init_tortoise(tortoise0) == 0){
		return 0;
	}

	if (!device_is_ready(can_dev)) {
		printf("CAN: Device %s not ready.\n", can_dev->name);
		return 0;
	}

	ret = can_start(can_dev);

	int x = 0;
	int pos = 0;
	while (1) {
		ret = gpio_pin_toggle_dt(&green_led);
		if (ret < 0) {
			return 0;
		}

		led_state = !led_state;
		printf("LED state: %s\n", led_state ? "ON" : "OFF");
		printf("3\n");
		k_msleep(SLEEP_TIME_MS);

		if((x++ % 5) == 0){
			// swap the tortoise posistion
			gpio_pin_set_dt(&tortoise0[0], 0);
			gpio_pin_set_dt(&tortoise0[1], 0);

			pos = !pos;
			if(pos){
				gpio_pin_set_dt(&tortoise0[0], 0);
				gpio_pin_set_dt(&tortoise0[1], 1);
			}else{
				gpio_pin_set_dt(&tortoise0[0], 1);
				gpio_pin_set_dt(&tortoise0[1], 0);
			}
		}
	}
	return 0;
}
