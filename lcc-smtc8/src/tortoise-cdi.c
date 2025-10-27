/*
 * tortoise-cdi.c
 *
 *  Created on: Oct 26, 2025
 *      Author: robert
 */

#include <stdlib.h>

#include "lcc-cdi-control.h"

const struct lcc_cdi_input segment_251_inputs[] = {
		{"Node Name", NULL, NULL, 0, 0, LCC_CDI_INPUT_TYPE_STRING, 63, 0},
		{"Node Description", NULL, NULL, 0, 0, LCC_CDI_INPUT_TYPE_STRING, 64, 1},
		{NULL, NULL, NULL, 0, 0, 0, 0, 0}
};

const struct lcc_cdi_group segment_251_groups[] = {
		{"Your name and description for this node", NULL, 0, 0, },
		{NULL, NULL, 0, 0, NULL}
};

const struct lcc_cdi_segment segment_251 = {
		.segment_space = 251,
		.segment_name = "Node ID",
		.groups = segment_251_groups,
		.inputs = {0}
};

const struct lcc_cdi_segment segments[] = {
		segment_251,
		{0}
};

const struct lcc_cdi_control smtc8_cdi_control = {
		.segments = segments,
		.simple_info = NULL
};
