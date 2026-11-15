/*
 * partition_utils.h
 *
 *  Created on: Mar 21, 2026
 *      Author: robert
 */

#ifndef PARTITION_UTILS_H_
#define PARTITION_UTILS_H_

#include <stddef.h>

/**
 * Read data out of a partition into the given memory location.
 * This requires the partition to be in flash.
 */
int partition_util_load(int partition_id, void* dest, size_t size);

/**
 * Save data to the given partition
 */
int partition_util_save(int partition_id, const void* src, size_t size);

/**
 * Load data from EEPROM.  Assumes that EEPROM has label of 'eeprom-0'
 */
int partition_util_load_eeprom(uint32_t offset, void* dest, size_t size);

/**
 * Save data to EEPROM
 */
int partition_util_save_eeprom(uint32_t offset, const void* src, size_t size);

#endif /* PARTITION_UTILS_H_ */
