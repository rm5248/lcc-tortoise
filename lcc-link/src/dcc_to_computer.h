#ifndef DCC_TO_COMPUTER_H
#define DCC_TO_COMPUTER_H

#include <zephyr/sys/ring_buffer.h>

struct dcc_packet{
	uint8_t len;
	uint8_t data[15];
} __attribute__ ((packed));

struct dcc_to_computer{
	uint8_t ring_buffer_outgoing[1024];
	struct ring_buf ringbuf_outgoing;
	const struct device* computer_uart;
	k_tid_t process_thread_tid;
	struct k_thread process_thread_data;
};

void dcc_to_computer_init(struct dcc_to_computer* dcc, const struct device* computer_uart);

void dcc_to_computer_add_packet(struct dcc_to_computer* dcc, const uint8_t* packet_bytes, int len);

#endif /* DCC_TO_COMPUTER_H */
