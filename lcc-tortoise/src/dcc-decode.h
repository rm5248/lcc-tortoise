/*
 * dcc-decode.h
 *
 *  Created on: Jun 17, 2024
 *      Author: robert
 */

#ifndef LCC_TORTOISE_ZEPHYR_SRC_DCC_DECODE_H_
#define LCC_TORTOISE_ZEPHYR_SRC_DCC_DECODE_H_

struct dcc_decoder{
	const struct gpio_dt_spec* gpio_pin;
	const struct gpio_dt_spec* led_pin;
};

extern struct dcc_decoder dcc_decode;

void dcc_decoder_thread(void*, void*, void*);

#endif /* LCC_TORTOISE_ZEPHYR_SRC_DCC_DECODE_H_ */
