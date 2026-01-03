#include "adc_utils.h"

int adc_util_reading_to_voltage(uint8_t adc_resolution_bits, int system_voltage, int adc_count){
	int max_resolution = 0;

	adc_resolution_bits--;
	while(adc_resolution_bits > 0){
		max_resolution |= (0x01 << adc_resolution_bits);
		adc_resolution_bits--;
	}

	return (float)system_voltage / (float)max_resolution * adc_count;
}

int adc_util_voltage_divider_input_voltage(uint32_t r1_value, uint32_t r2_value, int divider_voltage){
	int numerator = (r1_value + r2_value) * divider_voltage;
	int denominator = r2_value;

	return numerator / denominator;
}
