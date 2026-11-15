/*
 * partition_utils.c
 *
 *  Created on: Mar 21, 2026
 *      Author: robert
 */

#include <zephyr/storage/flash_map.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/eeprom.h>


#include "partition_utils.h"

int partition_util_load(int partition_id, void* dest, size_t size){
	const struct flash_area* storage_area = NULL;
	int ret = 0;

	if(flash_area_open(partition_id, &storage_area) < 0){
		return -1;
	}

	if(flash_area_read(storage_area, 0, dest, size) < 0){
		ret = -1;
	}

	flash_area_close(storage_area);
	if(ret){
		printf("Unable to load partition %d\n", partition_id);
	}

	return ret;
}

int partition_util_save(int partition_id, const void* src, size_t size){
	int ret = 0;
	const struct flash_area* storage_area = NULL;

	if(flash_area_open(partition_id, &storage_area) < 0){
		return -1;
	}

	ret = flash_area_erase(storage_area, 0, size);
	if(ret){
		printf("can't erase: %d\n", ret);
		goto out;
	}

	ret = flash_area_write(storage_area, 0, src, size);

out:
	flash_area_close(storage_area);
	if(ret){
		printf("Unable to save configs to flash partition %d: %d(%s)\n", partition_id, ret, strerror(-ret));
	}

	return ret;
}

#if DT_NODE_EXISTS(DT_ALIAS(eeprom_0))
int partition_util_load_eeprom(uint32_t offset, void* dest, size_t size){
	const struct device* const dev = DEVICE_DT_GET(DT_ALIAS(eeprom_0));
	if(!dev){
		return -EEXIST;
	}

	// read memory
	return eeprom_read(dev, offset, dest, size);
}

int partition_util_save_eeprom(uint32_t offset, const void* src, size_t size){
	const struct device* const dev = DEVICE_DT_GET(DT_ALIAS(eeprom_0));
	if(!dev){
		return -EEXIST;
	}

	// read memory
	return eeprom_write(dev, offset, src, size);
}
#endif
