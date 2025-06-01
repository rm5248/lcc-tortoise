/*
 * dcc-decode.h
 *
 *  Created on: Jun 17, 2024
 *      Author: robert
 */

#ifndef LCC_TORTOISE_ZEPHYR_SRC_DCC_DECODE_H_
#define LCC_TORTOISE_ZEPHYR_SRC_DCC_DECODE_H_

#include <zephyr/kernel.h>

struct dcc_decoder;
struct dcc_packet_parser;

struct dcc_decoder_stm32{
	struct k_msgq readings;
	uint32_t readings_data[128];
	struct dcc_decoder* dcc_decoder;
	struct dcc_packet_parser* packet_parser;
//	struct gpio_callback dcc_cb_data;
};

extern struct dcc_decoder_stm32 dcc_decode_ctx;

int dcc_decoder_init(struct dcc_decoder_stm32* decoder);

void dcc_decoder_disable();

#endif /* LCC_TORTOISE_ZEPHYR_SRC_DCC_DECODE_H_ */
