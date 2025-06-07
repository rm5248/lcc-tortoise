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
	struct can_frame parsed_frames[12];
	struct k_msgq tx_msgq;
	struct k_msgq parsed_msgq;
	k_tid_t tx_thread_tid;
	k_tid_t parse_thread_tid;
	char gridconnect_in[128];
	int gridconnect_in_pos;
	struct k_thread tx_thread_data;
	struct k_thread parse_thread_data;
	const struct device* can_dev;
	struct k_sem parse_sem;
};

/**
 * Init the computer to CAN.
 *
 * @param can_dev The CAN device to send data out on
 */
void computer_to_can_init(struct computer_to_can*, const struct device* can_dev);

/**
 * Append data to our buffer.  This data should be ASCII formatted gridconnect data
 */
int computer_to_can_append_data(struct computer_to_can* comp_to_can, void* data, int len);

/**
 * Attempt to transmit the given frame by putting it into the tx queue.
 */
void computer_to_can_tx_frame(struct computer_to_can* comp_to_can, struct can_frame* frame);

/**
 * Get the queue of parsed CAN frames that come from the gridconnect ASCII protocol.
 */
struct k_msgq* computer_to_can_parsed_queue(struct computer_to_can* comp_to_can);

#endif
