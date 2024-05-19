/*
 * tortoise.h
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#ifndef LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_
#define LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_

enum tortoise_position{
	NORMAL,
	REVERSED
};

struct tortoise {
	const struct gpio_dt_spec gpios[2];
	enum tortoise_position position;
};

/**
 * Initialize the GPIOs for the tortoise
 */
int tortoise_init(struct tortoise* tort);

/**
 * Set the position that the tortoise should go to
 */
int tortoise_set_position(struct tortoise* tort, enum tortoise_position position);

#endif /* LCC_TORTOISE_ZEPHYR_SRC_TORTOISE_H_ */
