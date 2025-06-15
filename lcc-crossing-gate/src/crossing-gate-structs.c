/*
 * crossing-gate-structs.c
 *
 *  Created on: Jun 9, 2025
 *      Author: robert
 */
#include "crossing-gate-structs.h"

#include "crossing-gate-structs.h"

int sensor_input_value(struct sensor_input* input){
	int val = -1;

	if(input->sensor_gpio){
		val = gpio_pin_get_dt(input->sensor_gpio);
		input->is_on = val;
	}

	return input->is_on;
}

int sensor_input_raw_value(struct sensor_input* input){
	int val = -1;

	if(input->sensor_gpio){
		val = gpio_pin_get_dt(input->sensor_gpio);
	}

	return val;
}

void sensor_input_handle_event(struct sensor_input* input, uint64_t event_id){
	if(input->event_id_on == event_id){
		input->is_on = 1;
	}else if(input->event_id_off == event_id){
		input->is_on = 0;
	}
}

int sensor_input_valid(struct sensor_input* input){
  if(input->sensor_gpio){
    return 1;
  }

  if(input->event_id_off > 0 &&
    input->event_id_on > 0){
      return 1;
    }

  return 0;
}

int switch_input_value(struct switch_input* input){
  if(input->switch_gpio){
      int val = gpio_pin_get_dt(input->switch_gpio);
      if(input->polarity == FLAG_POLARITY_ACTIVE_LOW){
        val = !val;
      }
      return val;
  }

  return input->current_pos;
}

void switch_input_handle_event(struct switch_input* input, uint64_t event_id){
	if(input->event_id_reverse == event_id){
		input->current_pos = 1;
	}else if(input->event_id_normal == event_id){
		input->current_pos = 0;
	}
}
