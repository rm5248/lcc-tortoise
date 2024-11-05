/*
 * can_to_computer.c
 *
 *  Created on: Nov 4, 2024
 *      Author: robert
 */
#include <zephyr/drivers/uart.h>

#include "can_to_computer.h"
#include "lcc-gridconnect.h"

K_THREAD_STACK_DEFINE(can_rx_stack, 512);

static void can_frame_receive_thread(void *arg1, void *unused2, void *unused3)
{
	static struct can_frame zephyr_can_frame_rx;
	static struct lcc_can_frame lcc_rx;
	struct can_to_computer* can_to_computer = arg1;
	char gridconnect_out_buffer[64];

	while(1){
		if(k_msgq_get(&can_to_computer->rx_msgq, &zephyr_can_frame_rx, K_FOREVER) == 0){
			memset(&lcc_rx, 0, sizeof(lcc_rx));
			lcc_rx.can_id = zephyr_can_frame_rx.id;
			lcc_rx.can_len = zephyr_can_frame_rx.dlc;
			memcpy(lcc_rx.data, zephyr_can_frame_rx.data, 8);

			if(lcc_canframe_to_gridconnect(&lcc_rx, gridconnect_out_buffer, sizeof(gridconnect_out_buffer)) != LCC_OK){
				continue;
			}

			ring_buf_put(&can_to_computer->ringbuf_outgoing, gridconnect_out_buffer, strlen(gridconnect_out_buffer));
			uart_irq_tx_enable(can_to_computer->computer_uart);
		}
	}
}

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

	can_to_computer->tx_thread_tid = k_thread_create(&can_to_computer->tx_thread_data,
			can_rx_stack,
			K_THREAD_STACK_SIZEOF(can_rx_stack),
			can_frame_receive_thread, can_to_computer, NULL, NULL,
			0, 0,
			K_NO_WAIT);
	if (!can_to_computer->tx_thread_tid) {
		printf("ERROR spawning tx to computer thread\n");
	}
}
