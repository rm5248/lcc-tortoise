/*
 * can_to_computer.c
 *
 *  Created on: Nov 4, 2024
 *      Author: robert
 */
#include <zephyr/drivers/uart.h>

#include "can_to_computer.h"
#include "lcc-gridconnect.h"

void can_to_computer_init(struct can_to_computer* can_to_computer, const struct device* can_dev, const struct device* computer_uart){
	can_to_computer->computer_uart = computer_uart;
	k_msgq_init(&can_to_computer->rx_msgq, can_to_computer->rx_msgq_data, sizeof(struct can_frame), 55);
	ring_buf_init(&can_to_computer->ringbuf_outgoing,
				sizeof(can_to_computer->ring_buffer_outgoing),
				can_to_computer->ring_buffer_outgoing);

	const struct can_filter filter = {
			.flags = CAN_FILTER_IDE,
			.id = 0x0,
			.mask = 0
	};
	int filter_id;

	filter_id = can_add_rx_filter_msgq(can_dev, &can_to_computer->rx_msgq, &filter);
	printf("Filter ID: %d\n", filter_id);
}

struct k_msgq* can_to_computer_msgq(struct can_to_computer* can_to_computer){
	return &can_to_computer->rx_msgq;
}

int can_to_computer_send_frame(struct can_to_computer* can_to_computer, struct can_frame* can_frame){
	struct lcc_can_frame frm = {0};
	frm.can_id = can_frame->id;
	frm.can_len = can_frame->dlc;
	memcpy(frm.data, can_frame->data, 8);

	return can_to_computer_send_lcc_frame(can_to_computer, &frm);
}

int can_to_computer_send_lcc_frame(struct can_to_computer* can_to_computer, struct lcc_can_frame* can_frame){
	char gridconnect_out_buffer[64];

	if(lcc_canframe_to_gridconnect(can_frame, gridconnect_out_buffer, sizeof(gridconnect_out_buffer)) != LCC_OK){
		return -1;
	}

	ring_buf_put(&can_to_computer->ringbuf_outgoing, gridconnect_out_buffer, strlen(gridconnect_out_buffer));
	ring_buf_put(&can_to_computer->ringbuf_outgoing, "\r\n", 2);
	uart_irq_tx_enable(can_to_computer->computer_uart);

	return 0;
}
