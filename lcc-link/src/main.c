/*
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>

#include "lcc-gridconnect.h"
#include "computer_to_can.h"
#include "can_to_computer.h"
#include "dcc-decode-stm32.h"
#include "dcc_to_computer.h"

#include "dcc-decoder.h"
#include "dcc-packet-parser.h"

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
const struct device *const can_usb_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_can));
const struct device *const dcc_usb_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_dcc));
const struct device *const console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
const struct gpio_dt_spec load_button  = GPIO_DT_SPEC_GET(DT_NODELABEL(load_switch), gpios);

static struct computer_to_can computer_to_can;
static struct can_to_computer can_to_computer;
static struct dcc_to_computer dcc_to_computer;
static uint32_t last_rx_dcc_msg;
static struct gpio_callback load_button_cb_data;
static uint32_t load_button_press = 0;
static uint32_t load_button_diff = 0;
static int in_bootloader_mode = 0;
static uint32_t last_rx_can_msg = 0;
static uint32_t last_tx_can_msg = 0;
struct lcc_context* lcc_ctx = NULL;

static void blink_status_led();
static void blink_activity_led();
K_THREAD_DEFINE(status_blink, 512, blink_status_led, NULL, NULL, NULL,
		7, 0, 0);
K_THREAD_DEFINE(activity_blink, 512, blink_activity_led, NULL, NULL, NULL,
		7, 0, 0);

// VID:PID
// 0483:5740

static void irq_handler_can_usb(const struct device *dev, void *user_data){
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
				int recv_len, rb_len;
				uint8_t buffer[64];

				recv_len = uart_fifo_read(dev, buffer, sizeof(buffer));
				if (recv_len < 0) {
//						LOG_ERR("Failed to read UART FIFO");
					recv_len = 0;
				};

				rb_len = computer_to_can_append_data(&computer_to_can, buffer, recv_len);
				if (rb_len < recv_len) {
					printf("Drop %u bytes", recv_len - rb_len);
				}
		}

		if (uart_irq_tx_ready(dev)) {
			uint8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(&can_to_computer.ringbuf_outgoing, buffer, sizeof(buffer));
			if (!rb_len) {
//				printf("Ring buffer empty, disable TX IRQ");
				uart_irq_tx_disable(dev);
				continue;
			}

			send_len = uart_fifo_fill(dev, buffer, rb_len);
			if (send_len < rb_len) {
				printf("Drop %d bytes\n", rb_len - send_len);
			}

//			printf("ringbuf -> tty fifo %d bytes\n", send_len);
		}
	}
}

static void irq_handler_dcc_usb(const struct device *dev, void *user_data){
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
			if (uart_irq_rx_ready(dev)) {
					int recv_len, rb_len;
					uint8_t buffer[64];

					recv_len = uart_fifo_read(dev, buffer, sizeof(buffer));
					if (recv_len < 0) {
	//						LOG_ERR("Failed to read UART FIFO");
						recv_len = 0;
					};
					// Drop it on the floor, since we can't do anything with DCC data(yet).
					// We may eventually allow you to configure the DCC packet filter
			}


			if (uart_irq_tx_ready(dev)) {
				uint8_t buffer[64];
				int rb_len, send_len;

				rb_len = ring_buf_get(&dcc_to_computer.ringbuf_outgoing, buffer, sizeof(buffer));
				if (!rb_len) {
	//				printf("Ring buffer empty, disable TX IRQ");
					uart_irq_tx_disable(dev);
					continue;
				}

				send_len = uart_fifo_fill(dev, buffer, rb_len);
				if (send_len < rb_len) {
					printf("Drop %d bytes\n", rb_len - send_len);
				}

	//			printf("ringbuf -> tty fifo %d bytes\n", send_len);
			}
		}
}

static void blink_status_led(){
	const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_NODELABEL(status_led), gpios);

	while(1){
		if(in_bootloader_mode){
			// LED should be on, wink off once every three seconds
			gpio_pin_set_dt(&status_led, 1);
			k_sleep(K_MSEC(3000));
			gpio_pin_set_dt(&status_led, 0);
			k_sleep(K_MSEC(100));
		}else{
			uint64_t diff = k_cycle_get_32() - last_rx_dcc_msg;

			if(k_cyc_to_ms_ceil32(diff) < 500){
				// Receiving DCC signal, double blink
				gpio_pin_set_dt(&status_led, 1);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&status_led, 0);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&status_led, 1);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&status_led, 0);
				k_sleep(K_MSEC(1500));
			}else{
				// No DCC signal, single blink
				gpio_pin_set_dt(&status_led, 1);
				k_sleep(K_MSEC(100));
				gpio_pin_set_dt(&status_led, 0);
				k_sleep(K_MSEC(1500));
			}
		}
	}
}

static void blink_activity_led(){
	const struct gpio_dt_spec activity_led = GPIO_DT_SPEC_GET(DT_NODELABEL(led2), gpios);

	if (!gpio_is_ready_dt(&activity_led)) {
		return;
	}

	if(gpio_pin_configure_dt(&activity_led, GPIO_OUTPUT_INACTIVE) < 0){
		return;
	}

	while(1){
		uint64_t diff_tx = k_cycle_get_32() - last_tx_can_msg;
		uint64_t diff_rx = k_cycle_get_32() - last_rx_can_msg;

		if(k_cyc_to_ms_ceil32(diff_tx) < 25){
			gpio_pin_set_dt(&activity_led, 1);
			k_sleep(K_MSEC(50));
			gpio_pin_set_dt(&activity_led, 0);
		}else if(k_cyc_to_ms_ceil32(diff_rx) < 25){
			gpio_pin_set_dt(&activity_led, 1);
			k_sleep(K_MSEC(50));
			gpio_pin_set_dt(&activity_led, 0);
		}
		k_sleep(K_MSEC(10));
	}
}

static void incoming_dcc(struct dcc_decoder* decoder, const uint8_t* packet_bytes, int len){
	last_rx_dcc_msg = k_cycle_get_32();

	dcc_to_computer_add_packet(&dcc_to_computer, packet_bytes, len);
}

static int do_usb_init(){
	int ret = 0;
	const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_NODELABEL(status_led), gpios);

	if (!device_is_ready(can_usb_dev)) {
		printf("CDC ACM device not ready\n");
		return -1;
	}

	if (!device_is_ready(dcc_usb_dev)) {
		printf("CDC ACM device not ready\n");
		return -1;
	}

	ret = usb_enable(NULL);
	while(ret != 0){
		gpio_pin_set_dt(&status_led, 1);
		k_msleep(200);
		gpio_pin_set_dt(&status_led, 0);
		k_msleep(200);
	}

//	while (true) {
//		uint32_t dtr = 0U;
//
//		uart_line_ctrl_get(console_dev, UART_LINE_CTRL_DTR, &dtr);
//		if (dtr) {
//			break;
//		} else {
//			/* Give CPU resources to low priority threads. */
//			k_sleep(K_MSEC(50));
//		}
//		gpio_pin_set_dt(&status_led, 1);
//	}
//
//	gpio_pin_set_dt(&status_led, 0);
//	printf("Console init done\n");

	return 0;
}

static void load_button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	int load_value = gpio_pin_get_dt(&load_button);

	if(load_value){
		load_button_press = k_cycle_get_32();
	}else{
		load_button_diff = k_cyc_to_ms_ceil32(k_cycle_get_32() - load_button_press);
		load_button_press = 0;
	}
}

static void handle_button(struct computer_to_can* computer_to_can, struct can_to_computer* can_to_computer){
	if(load_button_press == 0){
		return;
	}

	uint32_t load_button_curr_diff = k_cyc_to_ms_ceil32(k_cycle_get_32() - load_button_press);

	if(load_button_curr_diff > 2000 && !in_bootloader_mode){
		// User has held load button for at least two seconds, go into bootloader mode
		printf("Going into bootloader mode\n");
		in_bootloader_mode = 1;
		lcc_ctx = firmware_load(computer_to_can, can_to_computer);
	}
}

static void splash(){
	printf("LCC-Link starting up\n");
}

static void init_dcc(){
	dcc_to_computer_init(&dcc_to_computer, dcc_usb_dev);
	dcc_decoder_init(&dcc_decode_ctx);
	dcc_decoder_set_packet_callback(dcc_decode_ctx.dcc_decoder, incoming_dcc);
	uart_irq_callback_set(dcc_usb_dev, irq_handler_dcc_usb);
}

static void init_load_button(){
	if (!gpio_is_ready_dt(&load_button)) {
		return;
	}

	if(gpio_pin_configure_dt(&load_button, GPIO_INPUT) < 0){
		return;
	}

	if(gpio_pin_interrupt_configure_dt(&load_button, GPIO_INT_EDGE_BOTH) < 0){
		return;
	}

	gpio_init_callback(&load_button_cb_data, load_button_pressed, BIT(load_button.pin));
	gpio_add_callback(load_button.port, &load_button_cb_data);
}

static void parse_from_computer(struct computer_to_can* computer_to_can){
	static struct can_frame rx_frame;
	static struct lcc_can_frame lcc_rx;

	while(k_msgq_get(computer_to_can_parsed_queue(computer_to_can), &rx_frame, K_NO_WAIT) == 0){
		if(in_bootloader_mode){
			// Convert to LCC CAN frame and push to liblcc
			memset(&lcc_rx, 0, sizeof(lcc_rx));
			lcc_rx.can_id = rx_frame.id;
			lcc_rx.can_len = rx_frame.dlc;
			memcpy(lcc_rx.data, rx_frame.data, 8);

			lcc_context_incoming_frame(lcc_ctx, &lcc_rx);
		}else{
			// Send out to CAN
			computer_to_can_tx_frame(computer_to_can, &rx_frame);
		}
	}
}

static void main_loop(struct computer_to_can* computer_to_can, struct can_to_computer* can_to_computer){
	struct k_poll_event poll_data[3];

	k_poll_event_init(&poll_data[0],
			K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY,
			computer_to_can_parsed_queue(computer_to_can));
	k_poll_event_init(&poll_data[1],
			K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY,
			can_to_computer_msgq(can_to_computer));
	k_poll_event_init(&poll_data[2],
			K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY,
			&dcc_decode_ctx.readings);

	while (1) {
		int rc = k_poll(poll_data, ARRAY_SIZE(poll_data), K_MSEC(250));
		if(poll_data[0].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
			// A frame has been parsed from the computer
			parse_from_computer(computer_to_can);
			poll_data[0].state = K_POLL_STATE_NOT_READY;
			last_rx_can_msg = k_cycle_get_32();
		}else if(poll_data[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
			// A frame has been received from the CAN bus
			static struct can_frame rx_frame;
			while(k_msgq_get(can_to_computer_msgq(can_to_computer), &rx_frame, K_NO_WAIT) == 0){
				// If we are in bootloader mode, drop it like it's hot
				if(!in_bootloader_mode){
					can_to_computer_send_frame(can_to_computer, &rx_frame);
				}
			}
			poll_data[1].state = K_POLL_STATE_NOT_READY;
		}else if(poll_data[2].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
			uint32_t timediff;
			while(k_msgq_get(&dcc_decode_ctx.readings, &timediff, K_NO_WAIT) == 0){
				if(dcc_decoder_polarity_changed(dcc_decode_ctx.dcc_decoder, timediff) == 1){
					dcc_decoder_pump_packet(dcc_decode_ctx.dcc_decoder);
				}
			}
			poll_data[2].state = K_POLL_STATE_NOT_READY;
		}

		handle_button(computer_to_can, can_to_computer);
	}
}

int main(void)
{
	int ret;
	const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_NODELABEL(status_led), gpios);

	splash();

	if (!gpio_is_ready_dt(&status_led)) {
		return -1;
	}

	if(gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE) < 0){
		return -1;
	}

	if(do_usb_init() < 0){
		while(1){
			gpio_pin_set_dt(&status_led, 1);
			k_msleep(50);
			gpio_pin_set_dt(&status_led, 0);
			k_msleep(250);
		}
	}

	if (!device_is_ready(can_dev)) {
		printf("CAN: Device %s not ready.\n", can_dev->name);
		return 0;
	}

	ret = can_start(can_dev);
	if(ret != 0){
		printf("Can't start CAN\n");
		return 0;
	}

	computer_to_can_init(&computer_to_can, can_dev);
	can_to_computer_init(&can_to_computer, can_dev, can_usb_dev);

	uart_irq_callback_set(can_usb_dev, irq_handler_can_usb);
	uart_irq_rx_enable(can_usb_dev);

	init_dcc();
	init_load_button();
	load_button_press = 0;

	main_loop(&computer_to_can, &can_to_computer);

	return 0;
}
