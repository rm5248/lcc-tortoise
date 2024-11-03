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

struct k_work state_change_work;
enum can_state current_state;
struct can_bus_err_cnt current_err_cnt;
const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
const struct device *const can_usb_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_can));
const struct device *const dcc_usb_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_dcc));
const struct device *const console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static struct can_frame zephyr_can_frame_tx;
static struct can_frame zephyr_can_frame_rx;
static struct lcc_can_frame lcc_rx;

#define STATE_POLL_THREAD_STACK_SIZE 512
#define STATE_POLL_THREAD_PRIORITY 2

K_THREAD_STACK_DEFINE(poll_state_stack, STATE_POLL_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(tx_stack, 512);


struct k_thread poll_state_thread_data;
struct k_thread tx_thread_data;
CAN_MSGQ_DEFINE(rx_msgq, 55);
CAN_MSGQ_DEFINE(tx_msgq, 55);

uint8_t ring_buffer_incoming[1024];
struct ring_buf ringbuf_incoming;
uint8_t ring_buffer_outgoing[1024];
struct ring_buf ringbuf_outgoing;
char gridconnect_out_buffer[128];
char gridconnect_in_buffer[128];
int gridconnect_in_buffer_pos = 0;

// VID:PID
// 0483:5740

char *state_to_str(enum can_state state)
{
	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		return "error-active";
	case CAN_STATE_ERROR_WARNING:
		return "error-warning";
	case CAN_STATE_ERROR_PASSIVE:
		return "error-passive";
	case CAN_STATE_BUS_OFF:
		return "bus-off";
	case CAN_STATE_STOPPED:
		return "stopped";
	default:
		return "unknown";
	}
}

void poll_state_thread(void *unused1, void *unused2, void *unused3)
{
	struct can_bus_err_cnt err_cnt = {0, 0};
	struct can_bus_err_cnt err_cnt_prev = {0, 0};
	enum can_state state_prev = CAN_STATE_ERROR_ACTIVE;
	enum can_state state;
	int err;

	while (1) {
		err = can_get_state(can_dev, &state, &err_cnt);
		if (err != 0) {
			printf("Failed to get CAN controller state: %d", err);
			k_sleep(K_MSEC(100));
			continue;
		}

		if (err_cnt.tx_err_cnt != err_cnt_prev.tx_err_cnt ||
		    err_cnt.rx_err_cnt != err_cnt_prev.rx_err_cnt ||
		    state_prev != state) {

			err_cnt_prev.tx_err_cnt = err_cnt.tx_err_cnt;
			err_cnt_prev.rx_err_cnt = err_cnt.rx_err_cnt;
			state_prev = state;
			printf("state: %s\n"
			       "rx error count: %d\n"
			       "tx error count: %d\n",
			       state_to_str(state),
			       err_cnt.rx_err_cnt, err_cnt.tx_err_cnt);
		} else {
			k_sleep(K_MSEC(100));
		}
	}
}

void state_change_work_handler(struct k_work *work)
{
	printf("State Change ISR\nstate: %s\n"
	       "rx error count: %d\n"
	       "tx error count: %d\n",
		state_to_str(current_state),
		current_err_cnt.rx_err_cnt, current_err_cnt.tx_err_cnt);
}

void state_change_callback(const struct device *dev, enum can_state state,
			   struct can_bus_err_cnt err_cnt, void *user_data)
{
	struct k_work *work = (struct k_work *)user_data;

	ARG_UNUSED(dev);

	current_state = state;
	current_err_cnt = err_cnt;
	k_work_submit(work);
}

static void init_rx_queue(){
	const struct can_filter filter = {
			.flags = CAN_FILTER_IDE,
			.id = 0x0,
			.mask = 0
	};
	int filter_id;

	filter_id = can_add_rx_filter_msgq(can_dev, &rx_msgq, &filter);
	printf("Filter ID: %d\n", filter_id);
}

static void irq_handler_can_usb(const struct device *dev, void *user_data){
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
				int recv_len, rb_len;
				uint8_t buffer[64];
				size_t len = MIN(ring_buf_space_get(&ringbuf_incoming),
						 sizeof(buffer));

				if (len == 0) {
					/* Throttle because ring buffer is full */
					uart_irq_rx_disable(dev);
					continue;
				}

				recv_len = uart_fifo_read(dev, buffer, len);
				if (recv_len < 0) {
//						LOG_ERR("Failed to read UART FIFO");
					recv_len = 0;
				};

				rb_len = ring_buf_put(&ringbuf_incoming, buffer, recv_len);
				if (rb_len < recv_len) {
//						LOG_ERR("Drop %u bytes", recv_len - rb_len);
				}

//					LOG_DBG("tty fifo -> ringbuf %d bytes", rb_len);
//					if (rb_len) {
//						uart_irq_tx_enable(dev);
//					}
		}


		if (uart_irq_tx_ready(dev)) {
			uint8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(&ringbuf_outgoing, buffer, sizeof(buffer));
			if (!rb_len) {
//				LOG_DBG("Ring buffer empty, disable TX IRQ");
				uart_irq_tx_disable(dev);
				continue;
			}

			send_len = uart_fifo_fill(dev, buffer, rb_len);
			if (send_len < rb_len) {
//				LOG_ERR("Drop %d bytes", rb_len - send_len);
			}

//			LOG_DBG("ringbuf -> tty fifo %d bytes", send_len);
		}
	}
}

static void parse_data_from_host(){
	char gridconnect_bytes[32];
	memset(gridconnect_bytes, 0, sizeof(gridconnect_bytes));
	int bytes_pos = 0;
	int got_bytes = ring_buf_get(&ringbuf_incoming,
					gridconnect_in_buffer + gridconnect_in_buffer_pos,
					sizeof(gridconnect_in_buffer) - gridconnect_in_buffer_pos);
	if(got_bytes == 0){
		return;
	}
	gridconnect_in_buffer_pos += got_bytes;

	// See if we have a valid frame
	int push_bytes = 0;
	int has_full_frame = 0;
	int pos;
	for(pos = 0; pos < gridconnect_in_buffer_pos; pos++){
		if(gridconnect_in_buffer[pos] == ':'){
			push_bytes = 1;
		}

		if(push_bytes){
			gridconnect_bytes[bytes_pos] = gridconnect_in_buffer[pos];
			bytes_pos++;
		}

		if(bytes_pos > sizeof(gridconnect_bytes)){
			// error, too much data! purge everything
			gridconnect_in_buffer_pos = 0;
			return;
		}

		if(gridconnect_in_buffer[pos] == ';'){
			// we have a full frame at this point!
			has_full_frame = 1;
			break;
		}
	}

	if(has_full_frame){
		if(lcc_gridconnect_to_canframe(gridconnect_bytes, &lcc_rx) == LCC_OK){
			// Push out to the CAN bus
			memset(&zephyr_can_frame_tx, 0, sizeof(zephyr_can_frame_tx));
			zephyr_can_frame_tx.id = lcc_rx.can_id;
			zephyr_can_frame_tx.dlc = lcc_rx.can_len;
			zephyr_can_frame_tx.flags = CAN_FRAME_IDE;
			memcpy(zephyr_can_frame_tx.data, lcc_rx.data, 8);

			k_msgq_put(&tx_msgq, &zephyr_can_frame_tx, K_NO_WAIT);
		}

		// Move everything in our buffer down
		int newpos = 0;
		for(pos = pos + 1; pos < gridconnect_in_buffer_pos; pos++){
			gridconnect_in_buffer[newpos] = gridconnect_in_buffer[pos];
			newpos++;
		}
		gridconnect_in_buffer_pos = newpos;
	}
}

void tx_thread(void *unused1, void *unused2, void *unused3)
{
	struct can_frame tx_frame;

	while(1){
		if(k_msgq_get(&tx_msgq, &tx_frame, K_FOREVER) == 0){
			int stat = can_send(can_dev, &tx_frame, K_FOREVER, NULL, NULL );
		}
	}
}

static int do_console_init(){
	int ret = 0;
	const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_NODELABEL(status_led), gpios);

	if (!device_is_ready(can_usb_dev)) {
//		LOG_ERR("CDC ACM device not ready");
		return -1;
	}

	ret = usb_enable(NULL);
	while(ret != 0){
		gpio_pin_set_dt(&status_led, 1);
		k_msleep(200);
		gpio_pin_set_dt(&status_led, 0);
		k_msleep(200);
	}

	while (true) {
		uint32_t dtr = 0U;

		uart_line_ctrl_get(console_dev, UART_LINE_CTRL_DTR, &dtr);
		if (dtr) {
			break;
		} else {
			/* Give CPU resources to low priority threads. */
			k_sleep(K_MSEC(50));
		}
		gpio_pin_set_dt(&status_led, 1);
	}

	gpio_pin_set_dt(&status_led, 0);
	printf("Console init done\n");

	return 0;
}

int main(void)
{
	int ret;
	bool led_state = true;
	k_tid_t rx_tid, get_state_tid;
	k_tid_t tx_thread_tid;
	const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_NODELABEL(status_led), gpios);

	ring_buf_init(&ringbuf_incoming, sizeof(ring_buffer_incoming), ring_buffer_incoming);
	ring_buf_init(&ringbuf_outgoing, sizeof(ring_buffer_outgoing), ring_buffer_outgoing);

	if (!gpio_is_ready_dt(&status_led)) {
		return -1;
	}

	if(gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE) < 0){
		return -1;
	}

	if(do_console_init() < 0){
		while(1){
			gpio_pin_set_dt(&status_led, 1);
			k_msleep(50);
			gpio_pin_set_dt(&status_led, 0);
			k_msleep(50);
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

	if (!device_is_ready(dcc_usb_dev)) {
		return 0;
	}

	k_work_init(&state_change_work, state_change_work_handler);

	get_state_tid = k_thread_create(&poll_state_thread_data,
					poll_state_stack,
					K_THREAD_STACK_SIZEOF(poll_state_stack),
					poll_state_thread, NULL, NULL, NULL,
					STATE_POLL_THREAD_PRIORITY, 0,
					K_NO_WAIT);
	if (!get_state_tid) {
		printf("ERROR spawning poll_state_thread\n");
	}

	tx_thread_tid = k_thread_create(&tx_thread_data,
			tx_stack,
			K_THREAD_STACK_SIZEOF(tx_stack),
			tx_thread, NULL, NULL, NULL,
			0, 0,
			K_NO_WAIT);
	if (!tx_thread_tid) {
		printf("ERROR spawning tx thread\n");
	}

	can_set_state_change_callback(can_dev, state_change_callback, &state_change_work);

	init_rx_queue();

	uart_irq_callback_set(can_usb_dev, irq_handler_can_usb);

	uart_irq_rx_enable(can_usb_dev);

	while (1) {
//		gpio_pin_set_dt(&status_led, 1);
//		k_msleep(200);
//		gpio_pin_set_dt(&status_led, 0);
//		k_msleep(200);
//		gpio_pin_set_dt(&status_led, 1);
//		k_msleep(200);
//		gpio_pin_set_dt(&status_led, 0);
//		k_msleep(1000);

		// Receive a CAN frame if there is any
		if(k_msgq_get(&rx_msgq, &zephyr_can_frame_rx, K_NO_WAIT) == 0){
			memset(&lcc_rx, 0, sizeof(lcc_rx));
			lcc_rx.can_id = zephyr_can_frame_rx.id;
			lcc_rx.can_len = zephyr_can_frame_rx.dlc;
			memcpy(lcc_rx.data, zephyr_can_frame_rx.data, 8);

			if(lcc_canframe_to_gridconnect(&lcc_rx, gridconnect_out_buffer, sizeof(gridconnect_out_buffer)) != LCC_OK){
				continue;
			}

			ring_buf_put(&ringbuf_outgoing, gridconnect_out_buffer, sizeof(gridconnect_out_buffer));
			uart_irq_tx_enable(can_usb_dev);
		}

		// Parse a CAN message from the host if there is any
		parse_data_from_host();
		k_yield();
	}

	return 0;
}
