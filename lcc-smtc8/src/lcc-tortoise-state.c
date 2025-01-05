/*
 * lcc-tortoise-state.c
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/flash_map.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"
#include "dcc-decoder.h"

struct lcc_tortoise_state lcc_tortoise_state = {
		.green_led = GPIO_DT_SPEC_GET(DT_ALIAS(greenled), gpios),
		.blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(blueled), gpios),
		.gold_led = GPIO_DT_SPEC_GET(DT_ALIAS(goldled), gpios),
		.blue_button = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_switch), gpios),
		.gold_button = GPIO_DT_SPEC_GET(DT_NODELABEL(gold_switch), gpios),
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

	if(gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_BOTH) < 0){
		return -1;
	}

	// TODO callbacks on button press

	return 0;
}

int lcc_tortoise_state_init(){
	lcc_tortoise_state.blue_button_press = 0;
	lcc_tortoise_state.button_control = BUTTON_CONTROL_NORMAL;
	lcc_tortoise_state.allow_new_command = 1;

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

	lcc_tortoise_state.gpio_expander = DEVICE_DT_GET(DT_NODELABEL(gpio_expander));

	for(int x = 0; x < 8; x++){
		lcc_tortoise_state.tortoises[x].config = &lcc_tortoise_state.tortoise_config[x];

		if(tortoise_init(&lcc_tortoise_state.tortoises[x]) < 0){
			return -1;
		}
	}

	return 0;
}

void save_tortoise_positions(){
	if(!lcc_tortoise_state.save_tortoise_pos_on_shutdown){
		return;
	}

	const struct flash_area* location_storage_area = NULL;
	int id = FIXED_PARTITION_ID(location_partition);
	uint8_t data_to_save[8 * 4];
	for(int x = 0; x < 8; x++){
		data_to_save[x] = lcc_tortoise_state.tortoises[x].current_position;
		data_to_save[x + 8] = lcc_tortoise_state.tortoises[x].current_position;
		data_to_save[x + 16] = lcc_tortoise_state.tortoises[x].current_position;
		data_to_save[x + 24] = lcc_tortoise_state.tortoises[x].current_position;
	}

	if(flash_area_open(id, &location_storage_area) < 0){
		return;
	}

	if(flash_area_write(location_storage_area, 0, data_to_save, sizeof(data_to_save)) < 0){
		return;
	}
	flash_area_close(location_storage_area);
	lcc_tortoise_state.save_tortoise_pos_on_shutdown = 0;
	printf("saved\n");
}

void save_switch_tracker(){
	// We store 4 outputs per byte, so our buffer only needs to be 512 long
	static uint8_t save_buffer[512];

	if(!lcc_tortoise_state.switch_tracker_dirty){
		return;
	}

	const struct flash_area* switch_tracking_partition = NULL;
	int id = FIXED_PARTITION_ID(switch_tracking_partition);

	if(flash_area_open(id, &switch_tracking_partition) < 0){
		return;
	}

	int current_byte = 0;
	int current_shift = 6;
	memset(save_buffer, 0, sizeof(save_buffer));
	for(int x = 0; x < 2048; x++){
		switch(lcc_tortoise_state.trackers[x].current_pos){
		case SWITCH_UNKNOWN:
			break;
		case SWITCH_NORMAL:
			save_buffer[current_byte] |= (0x01 << current_shift);
			break;
		case SWITCH_REVERSED:
			save_buffer[current_byte] |= (0x02 << current_shift);
			break;
		}

		current_shift = current_shift - 2;
		if(current_shift < 0){
			current_shift = 6;
			current_byte++;
		}
	}

	// Save the buffer out
	if(flash_area_write(switch_tracking_partition, 0, save_buffer, sizeof(save_buffer)) < 0){
		return;
	}

	flash_area_close(switch_tracking_partition);
	printf("loc_S\n");
	lcc_tortoise_state.switch_tracker_dirty = 0;
}

void set_switch_tracker_dirty(){
	if(lcc_tortoise_state.switch_tracker_dirty){
		return;
	}

	const struct flash_area* switch_tracking_partition = NULL;
	int id = FIXED_PARTITION_ID(switch_tracking_partition);

	if(flash_area_open(id, &switch_tracking_partition) < 0){
		return;
	}

	if(flash_area_erase(switch_tracking_partition, 0, switch_tracking_partition->fa_size) < 0){
		return;
	}
	flash_area_close(switch_tracking_partition);
	printf("+");
	lcc_tortoise_state.switch_tracker_dirty = 1;
}
