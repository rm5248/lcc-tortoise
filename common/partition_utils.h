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
 * Read data out of a partition into the given memory location
 */
int partition_util_load(int partition_id, void* dest, size_t size);

/**
 * Save data to the given partition
 */
int partition_util_save(int partition_id, const void* src, size_t size);

#endif /* PARTITION_UTILS_H_ */
