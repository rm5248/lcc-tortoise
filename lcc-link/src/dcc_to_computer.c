/*
 * dcc_to_computer.c
 *
 *  Created on: May 31, 2025
 *      Author: robert
 */
#include <zephyr/drivers/uart.h>
#include <string.h>

#include "dcc_to_computer.h"
#include "dcc-decode-stm32.h"
#include "dcc-decoder.h"
#include "dcc-packet-parser.h"

K_THREAD_STACK_DEFINE(dcc_process_stack, 512);

static void dcc_process(void *arg1, void *unused2, void *unused3) {
	uint32_t timediff;

	while(k_msgq_get(&dcc_decode_ctx.readings, &timediff, K_NO_WAIT) == 0){
//		printf(".");
		if(dcc_decoder_polarity_changed(dcc_decode_ctx.dcc_decoder, timediff) == 1){
			dcc_decoder_pump_packet(dcc_decode_ctx.dcc_decoder);
		}
	}
}

void dcc_to_computer_init(struct dcc_to_computer* dcc, const struct device* computer_uart){
	ring_buf_init(&dcc->ringbuf_outgoing,
				sizeof(dcc->ring_buffer_outgoing),
				dcc->ring_buffer_outgoing);
	dcc->computer_uart = computer_uart;

//	dcc->process_thread_tid = k_thread_create(&dcc->process_thread_data,
//			dcc_process_stack,
//				K_THREAD_STACK_SIZEOF(dcc_process_stack),
//				dcc_process, NULL, NULL, NULL,
//				0, 0,
//				K_NO_WAIT);
//	if (!dcc->process_thread_tid) {
//		printf("ERROR spawning dcc process thread\n");
//	}
}

void dcc_to_computer_add_packet(struct dcc_to_computer* dcc, const uint8_t* packet_bytes, int len){
	struct dcc_packet packet = {0};
	packet.len = len;
	memcpy(packet.data, packet_bytes, len);

	uint32_t dtr = 0U;
	uart_line_ctrl_get(dcc->computer_uart, UART_LINE_CTRL_DTR, &dtr);
	if (!dtr) {
		return;
	}

	int put = ring_buf_put(&dcc->ringbuf_outgoing, &packet, sizeof(packet));
	if(put != sizeof(packet)){
		printf("Only put %d bytes", put);
	}
	uart_irq_tx_enable(dcc->computer_uart);
}
