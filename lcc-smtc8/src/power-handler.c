/*
 * power-handler.c
 *
 *  Created on: Aug 11, 2024
 *      Author: robert
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/adc.h>
#include <gpio/gpio_mcp23xxx.h>
#include <stm32g0xx_ll_adc.h>
#include <stm32g0xx_ll_gpio.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"

const static struct gpio_dt_spec volts_line = GPIO_DT_SPEC_GET(DT_NODELABEL(volts), gpios);
//const static struct adc_dt_spec voltage_adc = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));
static struct gpio_callback volts_cb_data;
static int save_required = 0;

//static void do_adc(){
//	k_sleep(K_MSEC(1000));
//
//	while(1){
//		uint32_t count = 0;
//		uint16_t buf;
//		struct adc_sequence sequence = {
//			.buffer = &buf,
//			/* buffer size in bytes, not number of samples */
//			.buffer_size = sizeof(buf),
//		};
//
//		adc_sequence_init_dt(&voltage_adc, &sequence);
//
//		int err = adc_read_dt(&voltage_adc, &sequence);
//		if (err < 0) {
//			printk("Could not read (%d)\n", err);
//			return;
//		}
//
//		int32_t val_mv = buf;
//		err = adc_raw_to_millivolts_dt(&voltage_adc,
//									   &val_mv);
//		if(err == 0){
//			printf("millivolts adc: %d\n", val_mv);
//
//			// Convert the mv to input volts
//			uint32_t vcc = val_mv *  (56000 + 15000) / 15000;
//			printf("vcc: %dmv\n", vcc);
//		}
//		k_sleep(K_MSEC(1000));
//	}
//}

//K_THREAD_DEFINE(adc_thread, 512, do_adc, NULL, NULL, NULL,
//		7, 0, 0);

static void power_lost_irq(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	const struct mcp23xxx_config* expander_config = lcc_tortoise_state.gpio_expander->config;
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

//	while(1){
//		printf(",");
//		k_msleep(1);
//	}
}

static void adc_irq_fn(void* arg)
{
	if(LL_ADC_IsActiveFlag_AWD1(ADC1)){
		LL_ADC_ClearFlag_AWD1(ADC1);
		const struct mcp23xxx_config* expander_config = lcc_tortoise_state.gpio_expander->config;
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
			flash_area_close(location_storage_area);
			save_required = 0;
			printf("saved\n");
		}
	}
}

void powerhandle_init(){
	__HAL_RCC_ADC_CLK_ENABLE();

	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_1, LL_GPIO_MODE_ANALOG);

	// Install our interrupt handler for the ADC
	IRQ_CONNECT(ADC1_COMP_IRQn, 0, adc_irq_fn, NULL, 0);
	irq_enable(ADC1_COMP_IRQn);

    /* Enable ADC internal voltage regulator */
    LL_ADC_EnableInternalRegulator(ADC1);
    while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0U){
    	k_sleep(K_USEC(2));
    }

	LL_ADC_SetClock(ADC1, LL_ADC_CLOCK_SYNC_PCLK_DIV2);
	LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
	LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_CONTINUOUS);

	LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, LL_ADC_SAMPLINGTIME_39CYCLES_5);
	LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_1);

	// Configure analog watchdog 1
	LL_ADC_SetAnalogWDMonitChannels(ADC1, LL_ADC_AWD1, LL_ADC_AWD_CHANNEL_1_REG);
	LL_ADC_ClearFlag_AWD1(ADC1);
	LL_ADC_ConfigAnalogWDThresholds(ADC1, LL_ADC_AWD1, 0xFFF, 1500);
	LL_ADC_EnableIT_AWD1(ADC1);

	// Sampling time
	LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_COMMON_1);

	// Now let's enable it and start the ADC running
	LL_ADC_Enable(ADC1);
	LL_ADC_REG_StartConversion(ADC1);

	// adc values:
	// 2130 = ~10v
	// 1800 = ~8.5v
	// 1500 = ~7v
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
