#ifndef COMPUTER_TO_CAN_H
#define COMPUTER_TO_CAN_H

#include <zephyr/drivers/can.h>
#include <zephyr/sys/ring_buffer.h>

/**
 * All of the variables required to turn a message from the computer
 * into a CAN message, and send it out
 */
struct computer_to_can{
	uint8_t ring_buffer_incoming[1024];
	struct ring_buf ringbuf_incoming;
	struct can_frame tx_msgq_data[55];
	struct k_msgq tx_msgq;
	k_tid_t tx_thread_tid;
	k_tid_t rx_thread_tid;
	char gridconnect_in[128];
	int gridconnect_in_pos;
	struct k_thread tx_thread_data;
	struct k_thread rx_thread_data;
	const struct device* can_dev;
	struct k_sem parse_semaphore;
};

void computer_to_can_init(struct computer_to_can*, const struct device* can_dev);

#endif
