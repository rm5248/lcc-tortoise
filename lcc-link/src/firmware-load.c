/*
 * firmware-load.c
 *
 *  Created on: May 31, 2025
 *      Author: robert
 */
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/uart.h>

#include "firmware-load.h"
#include "lcc.h"
#include "lcc-common.h"
#include "lcc-event.h"
#include "lcc-memory.h"
#include "lcc-firmware-upgrade.h"
#include "computer_to_can.h"
#include "can_to_computer.h"
#include "dcc-decode-stm32.h"
#include "lcc-gridconnect.h"

static const struct flash_area* slot1 = NULL;

static void firmware_upgrade_start(struct lcc_firmware_upgrade_context* ctx){
	int ret = flash_area_erase(slot1, 0, slot1->fa_size);
	if( ret < 0){
		flash_area_close(slot1);
		slot1 = NULL;
		printf("flash erase bad\n");
	}

	printf("firmware upgrade start\n");
}

static void firmware_upgrade_incoming_data(struct lcc_firmware_upgrade_context* ctx, uint32_t starting_address, void* data, int data_len){
	if(!slot1){
		lcc_firmware_write_error(ctx, LCC_ERRCODE_PERMANENT, NULL);
		return;
	}

	// Make sure that the memory is aligned
	uint32_t alignment = flash_area_align(slot1);
	void* data_to_write = data;
	uint8_t data_aligned[60];
	if(data_len % alignment){
		// The data is not aligned
		memset(data_aligned, 0, sizeof(data_aligned));
		memcpy(data_aligned, data, data_len);
		while(data_len % alignment){
			data_len++;
		}
		data_to_write = data_aligned;
	}

	int ret = flash_area_write(slot1, starting_address, data_to_write, data_len);
	if(ret == 0){
		lcc_firmware_write_ok(ctx);
	}else{
		printf("write err: %d starting addr 0x%08X.  Tried to write %d bytes\n", ret, starting_address, data_len);
		lcc_firmware_write_error(ctx, LCC_ERRCODE_PERMANENT, NULL);
	}
}

static void firmware_upgrade_finished(struct lcc_firmware_upgrade_context* ctx){
	printf("firmware upgrade finished\n");
	boot_set_pending(1);
	// Wait for the final message to flush before we reboot
	k_sleep(K_MSEC(50));
	sys_reboot(SYS_REBOOT_COLD);
}

static int lcc_write_cb(struct lcc_context* ctx, struct lcc_can_frame* lcc_frame){
	struct can_to_computer* can_to_computer = lcc_context_user_data(ctx);
	char gridconnect_out_buffer[64];

	if(lcc_canframe_to_gridconnect(lcc_frame, gridconnect_out_buffer, sizeof(gridconnect_out_buffer)) != LCC_OK){
		return -1;
	}

	ring_buf_put(&can_to_computer->ringbuf_outgoing, gridconnect_out_buffer, strlen(gridconnect_out_buffer));
	ring_buf_put(&can_to_computer->ringbuf_outgoing, "\r\n", 2);
	uart_irq_tx_enable(can_to_computer->computer_uart);

//	printf("lcc write cb\n");

	return LCC_OK;
}

static void mem_address_space_information_query(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space){
	// This memory space does not exist: return an error
	lcc_memory_respond_information_query(ctx, alias, 0, address_space, 0, 0, 0);
}

static void mem_address_space_read(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space, uint32_t starting_address, uint8_t read_count){
	lcc_memory_respond_read_reply_fail(ctx, alias, address_space, 0, 0, NULL);
}

void mem_address_space_write(struct lcc_memory_context* ctx, uint16_t alias, uint8_t address_space, uint32_t starting_address, void* data, int data_len){
	lcc_memory_respond_write_reply_fail(ctx, alias, address_space, starting_address, 0, NULL);
}

static void reboot(struct lcc_memory_context* ctx){
	sys_reboot(SYS_REBOOT_COLD);
}

struct lcc_context* firmware_load(struct computer_to_can* computer_to_can, struct can_to_computer* can_to_computer){
	// Disable the CAN and DCC so we don't do anything with those subsystems
//	can_stop(can_dev);
//	dcc_decoder_disable();

	const uint64_t lcc_id = 0x020202000000;

	struct lcc_context* ctx = lcc_context_new();
	lcc_context_set_unique_identifer( ctx,
			lcc_id );
	lcc_context_set_simple_node_information(ctx,
			"Snowball Creek Electronics",
			"LCC-Link",
			"R" CONFIG_BOARD_REVISION,
			"1");

	lcc_context_set_userdata(ctx, can_to_computer);

	lcc_context_set_write_function( ctx, lcc_write_cb, NULL );

	lcc_datagram_context_new(ctx);
	struct lcc_memory_context* mem_ctx = lcc_memory_new(ctx);
	// NOTE: currently bug in liblcc where CDI is required to exist otherwise firmware loading won't work
	const char* cdi = "<cdi></cdi>";
	lcc_memory_set_cdi(mem_ctx, cdi, strlen(cdi), 0);
	lcc_memory_set_memory_functions(mem_ctx,
	    mem_address_space_information_query,
	    mem_address_space_read,
	    mem_address_space_write);
	lcc_memory_set_reboot_function(mem_ctx, reboot);

	struct lcc_firmware_upgrade_context* fwu_ctx = lcc_firmware_upgrade_new(ctx);

	lcc_firmware_upgrade_set_functions(fwu_ctx,
			firmware_upgrade_start,
			firmware_upgrade_incoming_data,
			firmware_upgrade_finished);

	int id = FIXED_PARTITION_ID(slot1_partition);

	flash_area_open(id, &slot1);

	// TODO this should be more robust(check if alias is bad or good)
	// we're going to assume that since our network should be just us(and JMRI)
	// that there shouldn't be an alias collision
	int stat = lcc_context_generate_alias( ctx );
	lcc_context_claim_alias(ctx);

	// Now at this point, we just process packets that come in.
	// The pushing of packets is done in the main loop of the application

	return ctx;
}
