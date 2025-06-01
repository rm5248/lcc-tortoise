/*
 * firmware-load.h
 *
 *  Created on: May 31, 2025
 *      Author: robert
 */

#ifndef LCC_LINK_SRC_FIRMWARE_LOAD_H_
#define LCC_LINK_SRC_FIRMWARE_LOAD_H_

struct computer_to_can;

void firmware_load(struct computer_to_can* computer_to_can, const struct device *const can_dev);

#endif /* LCC_LINK_SRC_FIRMWARE_LOAD_H_ */
