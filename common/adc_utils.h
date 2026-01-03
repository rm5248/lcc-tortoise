/**
 * ADC Utils
 *
 * MIT license
 * https://github.com/rm5248/adc_utils
 */
#ifndef ADC_UTILS_H
#define ADC_UTILS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert an ADC reading to voltage
 *
 * @param adc_resolution_bits How many bits of resolution the ADC has.  In a 12-bit ADC, this should be the value 12
 * @param system_voltage The system voltage, in millivolts
 * @param adc_count The reading from the ADC
 * @return The ADC value in millivolts
 */
int adc_util_reading_to_voltage(uint8_t adc_resolution_bits, int system_voltage, int adc_count);

/**
 * Figure out the input voltage into a voltage divider
 *
 * @param r1_value Resistor 1 value, in ohms
 * @param r2_value Resistor 2 value, in ohms
 * @param divider_voltage Measured voltage of the voltage divider.  This should generally be the output of adc_util_reading_to_voltage.
 * This value should be in mv.
 * @return The input voltage into the voltage divider, in millivolts.
 */
int adc_util_voltage_divider_input_voltage(uint32_t r1_value, uint32_t r2_value, int divider_voltage);

#ifdef __cplusplus
} /* extern C */
#endif

#endif
