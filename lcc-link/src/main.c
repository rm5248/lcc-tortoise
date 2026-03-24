/*
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>

#include "lcc-gridconnect.h"
#include "computer_to_can.h"
#include "can_to_computer.h"
#include "dcc-decode-stm32.h"
#include "dcc_to_computer.h"
#include "firmware_upgrade.h"

#include "lcc.h"
#include "lcc-common.h"
#include "lcc-memory.h"
#include "lcc-datagram.h"
#include "partition_utils.h"

#include "dcc-decoder.h"
#include "dcc-packet-parser.h"

USBD_DEVICE_DEFINE(lcc_link_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   0x0483, 0x5740);

USBD_DESC_LANG_DEFINE(lcc_link_lang);
USBD_DESC_MANUFACTURER_DEFINE(lcc_link_mfr, "Snowball Creek Electronics");
USBD_DESC_PRODUCT_DEFINE(lcc_link_product, "LCC Link");

USBD_DESC_CONFIG_DEFINE(lcc_link_fs_cfg_desc, "FS Configuration");
USBD_CONFIGURATION_DEFINE(lcc_link_fs_config, 0, 100, &lcc_link_fs_cfg_desc);

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
const struct device *const can_usb_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_can));
const struct device *const dcc_usb_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_dcc));
const struct device *const console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static struct computer_to_can computer_to_can;
static struct can_to_computer can_to_computer;
static struct dcc_to_computer dcc_to_computer;
static uint32_t last_rx_dcc_msg;
static uint32_t last_rx_can_msg = 0;
static uint32_t last_tx_can_msg = 0;
struct lcc_context* lcc_ctx = NULL;
struct dcc_decoder_stm32 dcc_decode_ctx;

static void blink_status_led();
static void blink_activity_led();
K_THREAD_DEFINE(status_blink, 512, blink_status_led, NULL, NULL, NULL,
		7, 0, 0);
K_THREAD_DEFINE(activity_blink, 512, blink_activity_led, NULL, NULL, NULL,
		7, 0, 0);

// VID:PID
// 0483:5740
// This VID/PID combination is what is used by ST for their 'Virtual COM port'
// it is also used by RR-cirkits

static void irq_handler_can_usb(const struct device *dev, void *user_data){
	static uint8_t buffer[64];

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
				int recv_len, rb_len;

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
	static uint8_t buffer[64];

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
			if (uart_irq_rx_ready(dev)) {
					int recv_len, rb_len;

					recv_len = uart_fifo_read(dev, buffer, sizeof(buffer));
					if (recv_len < 0) {
	//						LOG_ERR("Failed to read UART FIFO");
						recv_len = 0;
					};
					// Drop it on the floor, since we can't do anything with DCC data(yet).
					// We may eventually allow you to configure the DCC packet filter
			}


			if (uart_irq_tx_ready(dev)) {
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

static void blink_activity_led(){
	const struct gpio_dt_spec rx_led = GPIO_DT_SPEC_GET(DT_NODELABEL(rx_led), gpios);
	const struct gpio_dt_spec tx_led = GPIO_DT_SPEC_GET(DT_NODELABEL(tx_led), gpios);

	if (!gpio_is_ready_dt(&rx_led)) {
		return;
	}

	if (!gpio_is_ready_dt(&tx_led)) {
		return;
	}

	if(gpio_pin_configure_dt(&rx_led, GPIO_OUTPUT) < 0){
		return;
	}
	if(gpio_pin_configure_dt(&tx_led, GPIO_OUTPUT) < 0){
		return;
	}

	while(1){
		uint64_t diff_tx = k_cycle_get_32() - last_tx_can_msg;
		uint64_t diff_rx = k_cycle_get_32() - last_rx_can_msg;
		int blinked = 0;

		if(k_cyc_to_ms_ceil32(diff_tx) < 25){
			gpio_pin_set_dt(&tx_led, 1);
			blinked = 1;
		}
		if(k_cyc_to_ms_ceil32(diff_rx) < 25){
			gpio_pin_set_dt(&rx_led, 1);
			blinked = 1;
		}

		if(blinked){
			k_sleep(K_MSEC(50));
			gpio_pin_set_dt(&tx_led, 0);
			gpio_pin_set_dt(&rx_led, 0);
		}
		k_sleep(K_MSEC(20));
	}
}

static void incoming_dcc(struct dcc_decoder* decoder, const uint8_t* packet_bytes, int len){
	last_rx_dcc_msg = k_cycle_get_32();

	dcc_to_computer_add_packet(&dcc_to_computer, packet_bytes, len);
}

static int do_usb_init(){
	int ret;

	if (!device_is_ready(can_usb_dev)) {
		printf("CDC ACM device not ready\n");
		return -1;
	}

	if (!device_is_ready(dcc_usb_dev)) {
		printf("CDC ACM device not ready\n");
		return -1;
	}

	ret = usbd_add_descriptor(&lcc_link_usbd, &lcc_link_lang);
	if (ret) return ret;

	ret = usbd_add_descriptor(&lcc_link_usbd, &lcc_link_mfr);
	if (ret) return ret;

	ret = usbd_add_descriptor(&lcc_link_usbd, &lcc_link_product);
	if (ret) return ret;

	ret = usbd_add_configuration(&lcc_link_usbd, USBD_SPEED_FS, &lcc_link_fs_config);
	if (ret) return ret;

	ret = usbd_register_all_classes(&lcc_link_usbd, USBD_SPEED_FS, 1, NULL);
	if (ret) return ret;

	usbd_device_set_code_triple(&lcc_link_usbd, USBD_SPEED_FS,
				    USB_BCC_MISCELLANEOUS, 0x02, 0x01);

	ret = usbd_init(&lcc_link_usbd);
	if (ret) return ret;

	ret = usbd_enable(&lcc_link_usbd);
	return ret;
}

static void splash(){
	printf("LCC-Link starting up\n");
}

static void init_dcc(){
	dcc_to_computer_init(&dcc_to_computer, dcc_usb_dev);
	dcc_decoder_init(&dcc_decode_ctx);
//	dcc_decoder_set_packet_callback(dcc_decode_ctx.dcc_decoder, incoming_dcc);
	uart_irq_callback_set(dcc_usb_dev, irq_handler_dcc_usb);

	dcc_decoder_set_packet_callback(dcc_decode_ctx.dcc_decoder, incoming_dcc);
}

static void parse_from_computer(struct computer_to_can* computer_to_can){
	static struct can_frame rx_frame;
	static struct lcc_can_frame lcc_rx;

	while(k_msgq_get(computer_to_can_parsed_queue(computer_to_can), &rx_frame, K_NO_WAIT) == 0){
		memset(&lcc_rx, 0, sizeof(lcc_rx));
		lcc_rx.can_id = rx_frame.id;
		lcc_rx.can_len = rx_frame.dlc;
		memcpy(lcc_rx.data, rx_frame.data, 8);

		// Send out to CAN
		computer_to_can_tx_frame(computer_to_can, &rx_frame);

		lcc_context_incoming_frame(lcc_ctx, &lcc_rx);
	}
}

static void main_loop(struct computer_to_can* computer_to_can, struct can_to_computer* can_to_computer){
	static struct k_poll_event poll_data[3] = {0};
	static struct k_timer alias_timer;

	k_timer_init(&alias_timer, NULL, NULL);
	k_timer_start(&alias_timer, K_MSEC(250), K_NO_WAIT);

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
			last_tx_can_msg = k_cycle_get_32();
		}else if(poll_data[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
			// A frame has been received from the CAN bus
			static struct can_frame rx_frame;
			while(k_msgq_get(can_to_computer_msgq(can_to_computer), &rx_frame, K_NO_WAIT) == 0){
				can_to_computer_send_frame(can_to_computer, &rx_frame);
			}
			poll_data[1].state = K_POLL_STATE_NOT_READY;
			last_rx_can_msg = k_cycle_get_32();
		}
		else if(poll_data[2].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
			uint32_t timediff;
			while(k_msgq_get(&dcc_decode_ctx.readings, &timediff, K_NO_WAIT) == 0){
				if(dcc_decoder_polarity_changed(dcc_decode_ctx.dcc_decoder, timediff) == 1){
					dcc_decoder_pump_packet(dcc_decode_ctx.dcc_decoder);
				}
			}
			poll_data[2].state = K_POLL_STATE_NOT_READY;
		}

		if(k_timer_status_get(&alias_timer) > 0 &&
			lcc_context_current_state(lcc_ctx) == LCC_STATE_INHIBITED){
			int stat = lcc_context_claim_alias(lcc_ctx);
			if(stat == LCC_ERROR_ALIAS_TX_NOT_EMPTY){
				k_timer_start(&alias_timer, K_MSEC(500), K_NO_WAIT);
				continue;
			}
			if(stat != LCC_OK){
			  // If we were unable to claim our alias, we need to generate a new one and start over
			  lcc_context_generate_alias(lcc_ctx);
			}else{
			  printf("Claimed alias %X\n", lcc_context_alias(lcc_ctx) );
			}
		}
	}
}

static int lcc_write_cb(struct lcc_context*, struct lcc_can_frame* lcc_frame){
	static struct can_frame zephyr_can_frame_tx;
	memset(&zephyr_can_frame_tx, 0, sizeof(zephyr_can_frame_tx));
	zephyr_can_frame_tx.id = lcc_frame->can_id;
	zephyr_can_frame_tx.dlc = lcc_frame->can_len;
	zephyr_can_frame_tx.flags = CAN_FRAME_IDE;
	memcpy(zephyr_can_frame_tx.data, lcc_frame->data, 8);

	can_to_computer_send_frame(&can_to_computer, &zephyr_can_frame_tx);

	return LCC_OK;
}

void mem_address_space_information_query(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space){
	// This memory space does not exist: return an error
	printf("%s:%d\n", __FILE__, __LINE__);
	lcc_memory_respond_information_query(ctx, alias, 0, address_space, 0, 0, 0);
}

void mem_address_space_read(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space, uint32_t starting_address, uint8_t read_count){
	printf("%s:%d\n", __FILE__, __LINE__);
	lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
}

void mem_address_space_write(struct lcc_memory_context *ctx, uint16_t alias,
		uint8_t address_space, uint32_t starting_address, void *data,
		int data_len) {
	printf("%s:%d\n", __FILE__, __LINE__);
	lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
}

static int enable_status_led(){
	struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_NODELABEL(status_led), gpios);

	if (!gpio_is_ready_dt(&status_led)) {
		return -1;
	}

	if(gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE) < 0){
		return -1;
	}

	return 0;
}

static void fail_usb(){
	struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_NODELABEL(status_led), gpios);

	while(1){
		gpio_pin_set_dt(&status_led, 1);
		k_msleep(50);
		gpio_pin_set_dt(&status_led, 0);
		k_msleep(250);
	}
}

int main(void)
{
	int ret;

	splash();

	if(enable_status_led() < 0){
		printf("unable to enable status LED\n");
		return -1;
	}

	if(do_usb_init() < 0){
		fail_usb();
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

	lcc_ctx = lcc_context_new();
	if(lcc_ctx == NULL){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return -1;
	}

	lcc_context_set_unique_identifer( lcc_ctx,
			0x020202000000 );
	lcc_context_set_simple_node_information(lcc_ctx,
			"Snowball Creek Electronics",
			"LCC-Link",
			"R" CONFIG_BOARD_REVISION,
			"0.1");

	lcc_context_set_write_function(lcc_ctx, lcc_write_cb, 0);
	// need datagram context and memory context in order to support firmware upgrades
	lcc_datagram_context_new(lcc_ctx);
	struct lcc_memory_context* mem_ctx = lcc_memory_new(lcc_ctx);
	static const char* cdi = "<cdi></cdi>";
	lcc_memory_set_cdi(mem_ctx, cdi, strlen(cdi), 0);
	lcc_memory_set_memory_functions(mem_ctx,
	    mem_address_space_information_query,
	    mem_address_space_read,
	    mem_address_space_write);

	firmware_upgrade_init(lcc_ctx);

	int stat = lcc_context_generate_alias(lcc_ctx);
	if(stat < 0){
		printf("error: can't gen alias: %d\n", stat);
		return 0;
	}

	main_loop(&computer_to_can, &can_to_computer);

	return 0;
}
