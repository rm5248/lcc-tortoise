/*
 * can_to_computer.h
 *
 *  Created on: Nov 4, 2024
 *      Author: robert
 */

#ifndef LCC_LINK_SRC_CAN_TO_COMPUTER_H_
#define LCC_LINK_SRC_CAN_TO_COMPUTER_H_

#include <zephyr/drivers/can.h>
#include <zephyr/sys/ring_buffer.h>

struct can_to_computer{
	k_tid_t tx_thread_tid;
	struct k_thread tx_thread_data;
	uint8_t ring_buffer_outgoing[1024];
	struct ring_buf ringbuf_outgoing;
	const struct device* computer_uart;
	struct can_frame rx_msgq_data[55];
	struct k_msgq rx_msgq;
};

void can_to_computer_init(struct can_to_computer*, const struct device* can_device, const struct device* computer_uart);

#endif /* LCC_LINK_SRC_CAN_TO_COMPUTER_H_ */
