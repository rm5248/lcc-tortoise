/*
 * birthing.c
 *
 *  Created on: Jan 5, 2025
 *      Author: robert
 */

#include <zephyr/drivers/gpio.h>
#include <stm32g0xx_ll_adc.h>
#include <stm32g0xx_ll_gpio.h>

#include "lcc-tortoise-state.h"
#include "birthing.h"
#include "tortoise.h"

static uint32_t vrefint;

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

static void init_adc(){
	__HAL_RCC_ADC_CLK_ENABLE();

	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_1, LL_GPIO_MODE_ANALOG);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_0, LL_GPIO_MODE_ANALOG);

    /* Enable ADC internal voltage regulator */
	LL_ADC_EnableInternalRegulator(ADC1);
	while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0U){
		k_sleep(K_USEC(2));
	}

	LL_ADC_SetClock(ADC1, LL_ADC_CLOCK_SYNC_PCLK_DIV2);
	LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
	LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
	LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);

	LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, LL_ADC_SAMPLINGTIME_39CYCLES_5);
//	LL_ADC_REG_SetSequencerChannels(ADC1, (LL_ADC_CHANNEL_0 |
//			LL_ADC_CHANNEL_1 |
//			LL_ADC_CHANNEL_VREFINT));

	// Sampling time
	LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_0, LL_ADC_SAMPLINGTIME_COMMON_1);
	LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_COMMON_1);

	// Now let's enable it and start the ADC running
	LL_ADC_Enable(ADC1);
}

static int adc_value(int channel){
	LL_ADC_REG_SetSequencerChannels(ADC1, channel);
	LL_ADC_REG_StartConversion(ADC1);

	while(!LL_ADC_IsActiveFlag_EOC(ADC1)){}
	int reading = LL_ADC_REG_ReadConversionData32(ADC1);
	while(!LL_ADC_IsActiveFlag_EOS(ADC1)){}

	return reading;
}

static uint32_t calc_voltage_divider(int r1, int r2, int counts_in){
	// Vchannelx = vref/full_scale * adc_data
	// see section 15.9 in the refence manual
	uint32_t voltage_mv = counts_in * 3300 / 4095;
	uint32_t analog_ref_voltage = __LL_ADC_CALC_VREFANALOG_VOLTAGE(vrefint, LL_ADC_RESOLUTION_12B);
	uint32_t voltage_mv2 = __LL_ADC_CALC_DATA_TO_VOLTAGE(analog_ref_voltage, counts_in, LL_ADC_RESOLUTION_12B);

	// the analog_ref_voltage seems to always be way too high for some reason, so the readings are all invalid.
//	printf("vrefint: %d vref: %d one: %d two: %d\n", vrefint, analog_ref_voltage, voltage_mv, voltage_mv2);

	return ((r1 + r2) * voltage_mv) / r2;
}

void smtc8_birthing(){
	init_adc();

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
			vrefint = adc_value(LL_ADC_CHANNEL_VREFINT);
			int volts = adc_value(LL_ADC_CHANNEL_0);
			int vin = adc_value(LL_ADC_CHANNEL_1);
			printf("State: %d Volts: %d(%dmv) VIN: %d(%dmv)\n",
					state,
					volts, (int)(volts * 1000 / 274.5),
					vin, vin * 1000 / 271);
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
