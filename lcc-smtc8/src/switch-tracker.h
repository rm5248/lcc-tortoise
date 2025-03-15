
#ifndef LCC_SMTC8_SRC_SWITCH_TRACKER_H_
#define LCC_SMTC8_SRC_SWITCH_TRACKER_H_

#include "dcc-packet-parser.h"

enum switch_position{
	SWITCH_UNKNOWN,
	SWITCH_NORMAL,
	SWITCH_REVERSED,
};

struct switch_tracker{
	enum switch_position current_pos;
};

/**
 * Initialize the switch tracker(tracks switches from DCC and sends out the
 * corresponding messages over LCC).
 */
void switch_tracker_init(int save_upon_shutdown);

void switch_tracker_incoming_switch_command(uint16_t accy_number, enum dcc_accessory_direction accy_dir);

#endif /* LCC_SMTC8_SRC_SWITCH_TRACKER_H_ */
