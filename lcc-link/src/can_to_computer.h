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

struct lcc_can_frame;

struct can_to_computer{
	uint8_t ring_buffer_outgoing[1024];
	struct ring_buf ringbuf_outgoing;
	const struct device* computer_uart;
	struct can_frame rx_msgq_data[55];
	struct k_msgq rx_msgq;
};

void can_to_computer_init(struct can_to_computer*, const struct device* can_device, const struct device* computer_uart);

struct k_msgq* can_to_computer_msgq(struct can_to_computer*);

int can_to_computer_send_frame(struct can_to_computer* can_to_computer, struct can_frame* can_frame);

int can_to_computer_send_lcc_frame(struct can_to_computer* can_to_computer, struct lcc_can_frame* can_frame);

#endif /* LCC_LINK_SRC_CAN_TO_COMPUTER_H_ */
