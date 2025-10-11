/*
 * firmware_upgrade.h
 *
 *  Created on: Aug 14, 2024
 *      Author: robert
 */

#ifndef FIRMWARE_UPGRADE_H_
#define FIRMWARE_UPGRADE_H_

typedef void (*firmware_upgrade_start)(void);
typedef void (*firmware_upgrade_success)(void);
typedef void (*firmware_upgrade_failure)(void);

struct lcc_context;

/**
 * Initialize the firmware upgrade handling.
 *
 * @param LCC context to use for firmware upgrades
 */
int firmware_upgrade_init(struct lcc_context*);

int firmware_upgrade_set_callbacks(firmware_upgrade_start upgrade_start,
                                   firmware_upgrade_success upgrade_success,
                                   firmware_upgrade_failure upgrade_fail);


#endif /* FIRMWARE_UPGRADE_H_ */
