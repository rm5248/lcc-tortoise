/*
 * firmware_upgrade.c
 *
 *  Created on: Aug 14, 2024
 *      Author: robert
 */


#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/kernel.h>
// zephyr/west does not find the bootutil header??
//#include "bootutil/bootutil_public.h"
#include <stdio.h>

#include "firmware_upgrade.h"
#include "lcc-common.h"
#include "lcc-firmware-upgrade.h"

static const struct flash_area* slot1 = NULL;
static firmware_upgrade_start start_cb = NULL;
static firmware_upgrade_success success_cb = NULL;
static firmware_upgrade_failure fail_cb = NULL;

static void firmware_upgrade_start_lcccb(struct lcc_firmware_upgrade_context* ctx){
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

	if(success_cb){
		success_cb();
	}

	// Wait for the final message to flush before we reboot
	k_sleep(K_MSEC(50));
	sys_reboot(SYS_REBOOT_COLD);
}

int firmware_upgrade_init(struct lcc_context* ctx){
	struct lcc_firmware_upgrade_context* fwu_ctx = lcc_firmware_upgrade_new(ctx);

	lcc_firmware_upgrade_set_functions(fwu_ctx,
			firmware_upgrade_start_lcccb,
			firmware_upgrade_incoming_data,
			firmware_upgrade_finished);

	int id = FIXED_PARTITION_ID(slot1_partition);

	return flash_area_open(id, &slot1);
}

int firmware_upgrade_set_callbacks(firmware_upgrade_start upgrade_start,
                                   firmware_upgrade_success upgrade_success,
                                   firmware_upgrade_failure upgrade_fail){
	start_cb = upgrade_start;
	success_cb = upgrade_success;
	fail_cb = upgrade_fail;

	return 0;
}
