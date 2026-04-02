/*
 * lcc-link-config.h
 *
 *  Created on: Mar 30, 2026
 *      Author: robert
 */

#ifndef LCC_LINK_SRC_LCC_LINK_CONFIG_H_
#define LCC_LINK_SRC_LCC_LINK_CONFIG_H_

#include <stdint.h>

/**
 * Node info - segment 251
 */
struct node_info_segment {
	char name[64];
	char description[64];
};

/**
 * DCC decoding info - segment 253
 *
 * Contains bytes that determine if we should(or should not) pass
 * the specified packet type up to the computer.
 *
 * In the future, this may also do things such as filtering the packets that we
 * want to send up to the computer(e.g. only show accessory commands or packets
 * for a specified address).
 */
struct dcc_decoding_info{
	uint8_t pass_idle_packets;
};

struct lcc_link_config{
	struct node_info_segment node_info;
	struct dcc_decoding_info dcc_decoding_info;
};

extern struct lcc_link_config lcc_link_data;

#endif /* LCC_LINK_SRC_LCC_LINK_CONFIG_H_ */
