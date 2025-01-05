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
#include "lcc-tortoise-state.h"

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

	int ret = flash_area_write(slot1, starting_address, data, data_len);
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
	save_tortoise_positions();
	save_switch_tracker();
	// Wait for the final message to flush before we reboot
	k_sleep(K_MSEC(50));
	sys_reboot(SYS_REBOOT_COLD);
}

int firmware_upgrade_init(struct lcc_context* ctx){
	struct lcc_firmware_upgrade_context* fwu_ctx = lcc_firmware_upgrade_new(ctx);

	lcc_firmware_upgrade_set_functions(fwu_ctx,
			firmware_upgrade_start,
			firmware_upgrade_incoming_data,
			firmware_upgrade_finished);

	int id = FIXED_PARTITION_ID(slot1_partition);

	return flash_area_open(id, &slot1);
}
