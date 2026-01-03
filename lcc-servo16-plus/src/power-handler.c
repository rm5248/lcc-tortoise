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
#include "adc_utils.h"

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
			     DT_SPEC_AND_COMMA)
};

static int powerhandle_vrefint;

static void adc_irq_fn(void* arg)
{
	if(LL_ADC_IsActiveFlag_AWD1(ADC1)){
		LL_ADC_ClearFlag_AWD1(ADC1);

		dcc_decoder_disable();
	}
}

void powerhandle_check_if_save_required(){

}

static uint32_t adc_value(int channel){
	LL_ADC_REG_SetSequencerChannels(ADC1, channel);
	LL_ADC_ClearFlag_EOC(ADC1);
	LL_ADC_REG_StartConversion(ADC1);

	while(!LL_ADC_IsActiveFlag_EOC(ADC1)){}
	uint32_t reading = LL_ADC_REG_ReadConversionData12(ADC1);
	printf("0x%X - 0x%08X\n", channel, reading);
	while(!LL_ADC_IsActiveFlag_EOS(ADC1)){}

	return reading;
}

void powerhandle_current_volts_mv(struct adc_readings* readings){
	int err;
	uint32_t count = 0;
	uint16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};

	/* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!adc_is_ready_dt(&adc_channels[i])) {
			printk("ADC controller device %s not ready\n", adc_channels[i].dev->name);
			return 0;
		}
	}

	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		int32_t val_mv;

		(void)adc_sequence_init_dt(&adc_channels[i], &sequence);

		err = adc_read_dt(&adc_channels[i], &sequence);
		if (err < 0) {
			printk("Could not read (%d)\n", err);
			continue;
		}

		/*
		 * If using differential mode, the 16 bit value
		 * in the ADC sample buffer should be a signed 2's
		 * complement value.
		 */
		if (adc_channels[i].channel_cfg.differential) {
			val_mv = (int32_t)((int16_t)buf);
		} else {
			val_mv = (int32_t)buf;
		}
		err = adc_raw_to_millivolts_dt(&adc_channels[i],
						   &val_mv);

		if(i == 0){
			readings->volts_mv = adc_util_voltage_divider_input_voltage(10000, 2700, val_mv);
		}else if(i == 1){
			readings->vin_mv = adc_util_voltage_divider_input_voltage(56000, 7500, val_mv);
		}else if(i == 2){
			readings->current = (int)((float)val_mv / 1.65);
		}
	}
}

void powerhandle_init(){}
