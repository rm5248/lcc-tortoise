/*
 * power-handler.c
 *
 *  Created on: Aug 11, 2024
 *      Author: robert
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/storage/flash_map.h>
#include <gpio/gpio_mcp23xxx.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"

const static struct gpio_dt_spec volts_line = GPIO_DT_SPEC_GET(DT_NODELABEL(volts), gpios);
static struct gpio_callback volts_cb_data;
static int save_required = 0;

static void power_lost_irq(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	struct mcp23xxx_config* expander_config = lcc_tortoise_state.gpio_expander->config;
	// Disable the GPIO expander in order to save power(maybe)
	gpio_pin_toggle_dt(&expander_config->gpio_reset);

	if(save_required){
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
	}

	while(1){
		printf(",");
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

void powerhandle_check_if_save_required(){
	int old_save_settings = save_required;
	int new_save_settings = 0;

	for(int x = 0; x < 8; x++){
		if(lcc_tortoise_state.tortoise_config[x].startup_control == STARTUP_LAST_POSITION){
			new_save_settings = 1;
		}
	}

	if(new_save_settings == 1 && old_save_settings == 0){
		// We need to save something - let's erase the flash just once.
		const struct flash_area* location_storage_area = NULL;
		int id = FIXED_PARTITION_ID(location_partition);

		if(flash_area_open(id, &location_storage_area) < 0){
			return;
		}

		if(flash_area_erase(location_storage_area, 0, location_storage_area->fa_size) < 0){

		}

		flash_area_close(location_storage_area);
	}

	save_required = new_save_settings;
}
