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

#include "dcc-decode-stm32.h"
#include "power-handler.h"

static void adc_irq_fn(void* arg)
{
	if(LL_ADC_IsActiveFlag_AWD1(ADC1)){
		LL_ADC_ClearFlag_AWD1(ADC1);

		dcc_decoder_disable();
	}
}

void powerhandle_init(){
	__HAL_RCC_ADC_CLK_ENABLE();

	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_1, LL_GPIO_MODE_ANALOG);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_0, LL_GPIO_MODE_ANALOG);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_ANALOG);

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
	LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_1 | LL_ADC_CHANNEL_0 | LL_ADC_CHANNEL_4);

	// Configure analog watchdog 1
	LL_ADC_SetAnalogWDMonitChannels(ADC1, LL_ADC_AWD1, LL_ADC_AWD_CHANNEL_1_REG);
	LL_ADC_ClearFlag_AWD1(ADC1);
	LL_ADC_ConfigAnalogWDThresholds(ADC1, LL_ADC_AWD1, 0xFFF, 1500);
	LL_ADC_EnableIT_AWD1(ADC1);

	// Sampling time
	LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_COMMON_1);
	LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_0, LL_ADC_SAMPLINGTIME_COMMON_1);
	LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_4, LL_ADC_SAMPLINGTIME_COMMON_1);

	// Now let's enable it and start the ADC running
	LL_ADC_Enable(ADC1);
	LL_ADC_REG_StartConversion(ADC1);
}

void powerhandle_check_if_save_required(){

}

static int adc_value(int channel){
	LL_ADC_REG_SetSequencerChannels(ADC1, channel);
	LL_ADC_ClearFlag_EOC(ADC1);
	LL_ADC_REG_StartConversion(ADC1);

	while(!LL_ADC_IsActiveFlag_EOC(ADC1)){}
	int reading = LL_ADC_REG_ReadConversionData32(ADC1);
	while(!LL_ADC_IsActiveFlag_EOS(ADC1)){}

	return reading;
}

void powerhandle_current_volts_mv(struct adc_readings* readings){
	LL_ADC_REG_StopConversion(ADC1);
	while(LL_ADC_REG_IsConversionOngoing(ADC1)){}

	LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
	int volts_count = adc_value(LL_ADC_CHANNEL_0);
	int vin_count = adc_value(LL_ADC_CHANNEL_1);
	int current_count = adc_value(LL_ADC_CHANNEL_4);

	LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_CONTINUOUS);
	LL_ADC_REG_StartConversion(ADC1);

	readings->volts_mv = (int)(volts_count * 1000 / 274.5);
	readings->vin_mv = vin_count * 1000 / 271;
	readings->current = current_count;
}
