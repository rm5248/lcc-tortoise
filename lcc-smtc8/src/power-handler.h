/*
 * power-handler.h
 *
 *  Created on: Aug 11, 2024
 *      Author: robert
 */

#ifndef LCC_TORTOISE_SRC_POWER_HANDLER_H_
#define LCC_TORTOISE_SRC_POWER_HANDLER_H_

struct adc_readings{
	int volts_mv;
	int vin_mv;
};

void powerhandle_init();

/**
 * When settings change, call this function to configure the power handling.
 * This is needed so that we don't write to the flash if we don't need to.
 */
void powerhandle_check_if_save_required();

void powerhandle_current_volts_mv(struct adc_readings*);

#endif /* LCC_TORTOISE_SRC_POWER_HANDLER_H_ */
