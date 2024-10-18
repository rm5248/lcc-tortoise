/*
 * lcc-tortoise-state.c
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/counter.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"
#include "dcc-decoder.h"


struct counter_alarm_cfg alarm_cfg;

struct lcc_tortoise_state lcc_tortoise_state = {
		.green_led = GPIO_DT_SPEC_GET(DT_ALIAS(greenled), gpios),
		.blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(blueled), gpios),
		.gold_led = GPIO_DT_SPEC_GET(DT_ALIAS(goldled), gpios),
		.blue_button = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_switch), gpios),
		.gold_button = GPIO_DT_SPEC_GET(DT_NODELABEL(gold_switch), gpios),
		.dcc_signal = GPIO_DT_SPEC_GET(DT_NODELABEL(dcc_pin), gpios),
		.tortoises = {
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise1), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise1), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise2), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise2), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise3), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise3), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise4), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise4), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise5), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise5), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise6), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise6), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise7), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise7), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
		}
};

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

	// TODO callbacks on button press

	return 0;
}

uint32_t readings[1000];
uint32_t pos = 0;

static void gpio_pin_change(const struct device *dev, struct gpio_callback *cb, uint32_t pins){
//printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
//	const uint32_t curr_cycle = k_cycle_get_32();
//	const uint32_t cycle_time_usec = k_cyc_to_us_floor32(curr_cycle - lcc_tortoise_state.prev_cycle);

	uint32_t current_ticks;
	counter_get_value(lcc_tortoise_state.dcc_counter, &current_ticks);
	uint32_t counter_diff;
	if(lcc_tortoise_state.prev_cycle > current_ticks){
		// rollover
		counter_diff = 0;
		counter_diff += (counter_get_top_value(lcc_tortoise_state.dcc_counter) - lcc_tortoise_state.prev_cycle);
		counter_diff += current_ticks;
	}else{
		counter_diff = current_ticks - lcc_tortoise_state.prev_cycle;
	}
	uint32_t cycle_time_usec = counter_ticks_to_us(lcc_tortoise_state.dcc_counter, counter_diff);

//	printf("curr %lu prev %lu cycles/usec %lu\n", curr_cycle, lcc_tortoise_state.prev_cycle, cycles_per_usec );

//	counter_get_value()

	lcc_tortoise_state.prev_cycle = current_ticks;

	int value = gpio_pin_get_dt(&lcc_tortoise_state.dcc_signal);
//	gpio_pin_set_dt(&lcc_tortoise_state.gold_led, value);
//	gpio_pin_set_dt(&lcc_tortoise_state.blue_led, value);

//	dcc_decoder_polarity_changed(lcc_tortoise_state.dcc_decoder, cycle_time_usec);

	readings[pos++] = cycle_time_usec;
	if(pos == (sizeof(readings)/sizeof(readings[0]))){
		pos = 0;
		printf("readings:\n");
		int num_readings_good = 0;
		int num_readings_bad = 0;
		for(int x = 0; x < sizeof(readings)/sizeof(readings[0]); x++){
//			printf("%lu,", readings[x]);
			if(readings[x] >= 52 && readings[x] <= 64){
				num_readings_good++;
//				printf("1");
			}else if(readings[x] >= 90 && readings[x] <= 200){
				num_readings_good++;
//				printf("0");
			}else{
				num_readings_bad++;
//				printf("-");
			}
		}
		printf("good: %d bad: %d", num_readings_good, num_readings_bad);
		printf("\n");

//		gpio_remove_callback(lcc_tortoise_state.dcc_signal.port, &lcc_tortoise_state.dcc_cb_data);
	}

}

static void gpio_pin_high(const struct device *dev, struct gpio_callback *cb, uint32_t pins){
//printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	const uint32_t cycles_per_usec = sys_clock_hw_cycles_per_sec() / USEC_PER_SEC;
	uint32_t curr_cycle = k_cycle_get_32();
	uint32_t cycle_time_usec = (curr_cycle - lcc_tortoise_state.prev_cycle) / cycles_per_usec;

//	printf("curr %lu prev %lu cycles/usec %lu\n", curr_cycle, lcc_tortoise_state.prev_cycle, cycles_per_usec );

	lcc_tortoise_state.prev_cycle = curr_cycle;

	dcc_decoder_rising_or_falling(lcc_tortoise_state.dcc_decoder, cycle_time_usec);

	readings[pos++] = cycle_time_usec;
	if(pos == (sizeof(readings)/sizeof(readings[0]))){
		pos = 0;
		printf("readings:\n");
		for(int x = 0; x < sizeof(readings)/sizeof(readings[0]); x++){
			printf("%lu,", readings[x]);
		}
		printf("\n");
	}

}

static void test_counter_interrupt_fn(const struct device *counter_dev,
				      uint8_t chan_id, uint32_t ticks,
				      void *user_data)
{
	uint32_t now_ticks;
	uint64_t now_usec;
	int now_sec;
	int err;

	err = counter_get_value(counter_dev, &now_ticks);
	if (err) {
		printk("Failed to read counter value (err %d)", err);
		return;
	}

	now_usec = counter_ticks_to_us(counter_dev, now_ticks);
	now_sec = (int)(now_usec / USEC_PER_SEC);

//	printk("!!! Alarm !!!\n");
//	printk("Now: %u\n", now_sec);

	gpio_pin_toggle_dt(&lcc_tortoise_state.gold_led);
	counter_set_channel_alarm(lcc_tortoise_state.dcc_counter , 0, &alarm_cfg);
}

int lcc_tortoise_state_init(){
	int gpio_int_type = GPIO_INT_EDGE_BOTH;

	if(init_led(&lcc_tortoise_state.green_led) < 0){
		return -1;
	}
	if(init_led(&lcc_tortoise_state.gold_led) < 0){
		return -1;
	}
	if(init_led(&lcc_tortoise_state.blue_led) < 0){
		return -1;
	}
	if(init_button(&lcc_tortoise_state.blue_button) < 0){
		return -1;
	}
	if(init_button(&lcc_tortoise_state.gold_button) < 0){
		return -1;
	}

	{
		const struct gpio_dt_spec* signal = &lcc_tortoise_state.dcc_signal;
		if (!gpio_is_ready_dt(signal)) {
			return -1;
		}

		if(gpio_pin_configure_dt(signal, GPIO_INPUT) < 0){
			return -1;
		}

		if(gpio_pin_interrupt_configure_dt(signal, gpio_int_type) < 0){
			return -1;
		}
	}

	if(gpio_int_type == GPIO_INT_EDGE_BOTH){
		gpio_init_callback(&lcc_tortoise_state.dcc_cb_data, gpio_pin_change, BIT(lcc_tortoise_state.dcc_signal.pin));
	}else if(gpio_int_type == GPIO_INT_EDGE_RISING){
		gpio_init_callback(&lcc_tortoise_state.dcc_cb_data, gpio_pin_high, BIT(lcc_tortoise_state.dcc_signal.pin));
	}
	gpio_add_callback(lcc_tortoise_state.dcc_signal.port, &lcc_tortoise_state.dcc_cb_data);

	lcc_tortoise_state.dcc_counter = DEVICE_DT_GET(DT_NODELABEL(dcc_counter));

	counter_start(lcc_tortoise_state.dcc_counter);

	for(int x = 0; x < 8; x++){
		lcc_tortoise_state.tortoises[x].config = &lcc_tortoise_state.tortoise_config[x];

		if(tortoise_init(&lcc_tortoise_state.tortoises[x]) < 0){
			return -1;
		}
	}

	return 0;
}
