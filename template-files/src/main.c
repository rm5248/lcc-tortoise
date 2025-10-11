/*
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/console/console.h>
#include <zephyr/dfu/mcuboot.h>

#include "firmware_upgrade.h"
#include "lcc.h"

#define VERSION_STR "0"

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static struct lcc_can_frame lcc_rx;
static struct lcc_context* lcc_ctx;

static struct gpio_callback blue_button_cb_data;
static struct gpio_callback gold_button_cb_data;

const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(DT_ALIAS(greenled), gpios);
const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(blueled), gpios);
const struct gpio_dt_spec gold_led = GPIO_DT_SPEC_GET(DT_ALIAS(goldled), gpios);

K_THREAD_STACK_DEFINE(tx_stack, 512);
struct k_thread tx_thread_data;
k_tid_t tx_thread_tid;
CAN_MSGQ_DEFINE(rx_msgq, 55);
CAN_MSGQ_DEFINE(tx_msgq, 12);

static void blink_led_green(){
	while(1){
		gpio_pin_set_dt(&green_led, 1);
		k_sleep(K_MSEC(100));
		gpio_pin_set_dt(&green_led, 0);
		k_sleep(K_MSEC(1500));
	}
}

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

static void can_frame_send_thread(void *unused1, void *unused2, void *unused3)
{
	static struct can_frame tx_frame;
	struct can_bus_err_cnt err_cnt = {0, 0};
	enum can_state state;

	while(1){
		if(k_msgq_get(&tx_msgq, &tx_frame, K_FOREVER) == 0){
			int err = can_get_state(can_dev, &state, &err_cnt);
			if(err != 0){
				printf("Can't get CAN state\n");
				continue;
			}

			if(state == CAN_STATE_BUS_OFF){
				printf("CAN bus off, dropping packet\n");
				continue;
			}

			int stat = can_send(can_dev, &tx_frame, K_FOREVER, NULL, NULL );
			if(stat < 0){
				printf("Unable to send: %d\n", stat);
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

	if(k_msgq_put(&tx_msgq, &zephyr_can_frame_tx, K_NO_WAIT) < 0){
		return LCC_ERROR_TX;
	}

	return LCC_OK;
}

static void init_can_txrx(){
	const struct can_filter filter = {
			.flags = CAN_FILTER_IDE,
			.id = 0x0,
			.mask = 0
	};
	int filter_id;

	filter_id = can_add_rx_filter_msgq(can_dev, &rx_msgq, &filter);
	printf("Filter ID: %d\n", filter_id);

	tx_thread_tid = k_thread_create(&tx_thread_data,
				tx_stack,
				K_THREAD_STACK_SIZEOF(tx_stack),
				can_frame_send_thread, NULL, NULL, NULL,
				0, 0,
				K_NO_WAIT);
	if (!tx_thread_tid) {
		printf("ERROR spawning tx thread\n");
	}
}

static void reboot(struct lcc_memory_context* ctx){
	sys_reboot(SYS_REBOOT_COLD);
}

static void factory_reset(struct lcc_memory_context* ctx){
}

static uint64_t load_lcc_id(){
	uint64_t ret = 0;
	int do_reboot = 0;

	const struct flash_area* lcc_storage_area = NULL;
	int id = FIXED_PARTITION_ID(lcc_partition);

	if(flash_area_open(id, &lcc_storage_area) < 0){
		return 1;
	}

	if(flash_area_read(lcc_storage_area, 0, &ret, sizeof(ret)) < 0){
		return 1;
	}

	// Check to see if the first 8 bytes are 0xFF.  If so, we assume the data is uninitialized
	const uint8_t mem[8] = {
			0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF
	};
	if(memcmp(&ret, mem, 8) == 0){
		uint64_t new_id = 0;
		int valid = 0;

		console_getline_init();

		// Do any birthing checks here to see if it's good or bad

		while(!valid){
			printf("LCC ID blank!  Please input lower 2 bytes of LCC ID as a single hex string followed by enter:\n");
			char id_buffer[32];
			char* console_data = console_getline();
			printf("console line: %s\n", console_data);
			new_id = strtoull(console_data, NULL, 16);
			new_id = new_id & 0xFFFF;
			new_id = new_id | (0x02020201ull << 16);
			lcc_node_id_to_dotted_format(new_id, id_buffer, sizeof(id_buffer));

			printf("New node ID will be %s.  Type 'y'<ENTER> to accept\n", id_buffer);
			console_data = console_getline();
			printf("response: %s\n", console_data);
			if(console_data[0] == 'y'){
				valid = 1;
				ret = new_id;
			}
		}

		flash_area_write(lcc_storage_area, 0, &ret, sizeof(ret));

		// Do a 'factory reset' to make sure everything is initialized
		factory_reset(NULL);
		do_reboot = 1;
	}

out:
	flash_area_close(lcc_storage_area);

	if(do_reboot){
		printf("Birthing complete, rebooting...\n");
		sys_reboot(SYS_REBOOT_COLD);
	}

	return ret;
}

static void main_loop(){
	struct k_poll_event poll_data[1];
	struct k_timer alias_timer;
	struct can_frame rx_frame;

	k_poll_event_init(&poll_data[0],
			K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY,
			&rx_msgq);

	k_timer_init(&alias_timer, NULL, NULL);
	k_timer_start(&alias_timer, K_MSEC(250), K_NO_WAIT);

	while (1) {
		int rc = k_poll(poll_data, ARRAY_SIZE(poll_data), K_MSEC(250));
		if(poll_data[0].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
			// Receive a CAN frame if there is any
			while(k_msgq_get(&rx_msgq, &rx_frame, K_NO_WAIT) == 0){
				memset(&lcc_rx, 0, sizeof(lcc_rx));
				lcc_rx.can_id = rx_frame.id;
				lcc_rx.can_len = rx_frame.dlc;
				memcpy(lcc_rx.data, rx_frame.data, 8);

				lcc_context_incoming_frame(lcc_ctx, &lcc_rx);
			}
			poll_data[0].state = K_POLL_STATE_NOT_READY;
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

static int tx_queue_size_cb(struct lcc_context*){
	return k_msgq_num_free_get(&tx_msgq);
}

static void splash(){
	printf("lcc-servo-16plus\n");
	printf("  Version: " VERSION_STR);
	printf("  Rev: R" CONFIG_BOARD_REVISION "\n");

	struct mcuboot_img_header versions[2];
	struct mcuboot_img_sem_ver semver[2] = {0};

	if(boot_read_bank_header(FIXED_PARTITION_ID(slot0_partition), &versions[0], sizeof(struct mcuboot_img_header)) == 0){
	  semver[0] = versions[0].h.v1.sem_ver;
	}
	if(boot_read_bank_header(FIXED_PARTITION_ID(slot1_partition), &versions[1], sizeof(struct mcuboot_img_header)) == 0){
	  semver[1] = versions[1].h.v1.sem_ver;
	}

	printf("  This slot version: %d.%d.%d\n", semver[0].major, semver[0].minor, semver[0].revision);
	printf("  Secondary slot version: %d.%d.%d\n", semver[1].major, semver[1].minor, semver[1].revision);
}

int main(void)
{
	int ret;

	// Sleep for a bit before we start to allow power to become stable.
	// This is probably not needed, but it shouldn't hurt.
	// This will also give other devices on the network a chance to come up.
	k_sleep(K_MSEC(250));

	splash();

	if (!device_is_ready(can_dev)) {
		printf("CAN: Device %s not ready.\n", can_dev->name);
		return 0;
	}

	ret = can_start(can_dev);
	if(ret != 0){
		printf("Can't start CAN\n");
		return 0;
	}

	init_can_txrx();

	uint64_t lcc_id = load_lcc_id();
	struct lcc_context* ctx = lcc_context_new();
	lcc_context_set_unique_identifer( ctx,
			lcc_id );
	lcc_context_set_simple_node_information(ctx,
			"Snowball Creek Electronics",
			"SMTC-8",
			"R" CONFIG_BOARD_REVISION,
			VERSION_STR);

	lcc_context_set_write_function( ctx, lcc_write_cb, tx_queue_size_cb );
	struct lcc_event_context* evt_ctx = lcc_event_new(ctx);

	lcc_datagram_context_new(ctx);
	struct lcc_memory_context* mem_ctx = lcc_memory_new(ctx);
//	lcc_memory_set_cdi(mem_ctx, cdi, sizeof(cdi), 0);
//	lcc_memory_set_memory_functions(mem_ctx,
//	    mem_address_space_information_query,
//	    mem_address_space_read,
//	    mem_address_space_write);
	lcc_memory_set_reboot_function(mem_ctx, reboot);
	lcc_memory_set_factory_reset_function(mem_ctx, factory_reset);

	firmware_upgrade_init(ctx);

	int stat = lcc_context_generate_alias( ctx );
	if(stat < 0){
		printf("error: can't gen alias: %d\n", stat);
		return 0;
	}

	// Init our callbacks
	//gpio_init_callback(&gold_button_cb_data, gold_button_pressed, BIT(lcc_tortoise_state.gold_button.pin));
	//gpio_add_callback(lcc_tortoise_state.gold_button.port, &gold_button_cb_data);
	//gpio_init_callback(&blue_button_cb_data, blue_button_pressed, BIT(lcc_tortoise_state.blue_button.pin));
	//gpio_add_callback(lcc_tortoise_state.blue_button.port, &blue_button_cb_data);

	main_loop();

	return 0;
}

