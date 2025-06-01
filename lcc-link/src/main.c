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


const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
const struct device *const can_usb_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_can));
const struct device *const dcc_usb_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_dcc));
const struct device *const console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));


static struct computer_to_can computer_to_can;
static struct can_to_computer can_to_computer;

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

				rb_len = ring_buf_put(&computer_to_can.ringbuf_incoming, buffer, recv_len);
				if (rb_len < recv_len) {
					printf("Drop %u bytes", recv_len - rb_len);
				}
				k_sem_give(&computer_to_can.parse_semaphore);
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

static void splash(){
	printf("LCC-Link starting up\n");
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

	while (1) {
		gpio_pin_set_dt(&status_led, 1);
		k_msleep(200);
		gpio_pin_set_dt(&status_led, 0);
		k_msleep(200);
		gpio_pin_set_dt(&status_led, 1);
		k_msleep(200);
		gpio_pin_set_dt(&status_led, 0);
		k_msleep(1000);
	}

	return 0;
}
