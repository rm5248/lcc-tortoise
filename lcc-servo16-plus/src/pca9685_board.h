/*
 * pca9685_board.h
 *
 *  Created on: Oct 24, 2025
 *      Author: robert
 */

#ifndef LCC_SERVO16_PLUS_SRC_PCA9685_BOARD_H_
#define LCC_SERVO16_PLUS_SRC_PCA9685_BOARD_H_

#include <stdint.h>

struct Board;

/**
 * For this board, handle the event and see if it should do anything.
 *
 * Returns 1 if the event was handled, 0 if not
 */
int pca9685_board_handle_event(struct Board* board, uint64_t event_id);

#endif /* LCC_SERVO16_PLUS_SRC_PCA9685_BOARD_H_ */
