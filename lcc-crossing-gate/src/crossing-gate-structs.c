/*
 * crossing-gate-structs.c
 *
 *  Created on: Jun 9, 2025
 *      Author: robert
 */
#include "crossing-gate-structs.h"
#include "crossing-gate.h"

void sensor_input_init(struct sensor_input* input, struct gpio_dt_spec* gpio){
	input->sensor_gpio = NULL;
	if(input->config->sensor_type == 0){
		// board input
		input->sensor_gpio = gpio;
	}
}

int sensor_input_value(struct sensor_input* input){
	int val = -1;

	if(input->sensor_gpio){
		val = gpio_pin_get_dt(input->sensor_gpio);
		if(input->config->polarity == 1){
			val = !val;
		}
		input->is_on = val;
	}

	return input->is_on;
}

int sensor_input_raw_value(struct sensor_input* input){
	int val = -1;

	if(input->sensor_gpio){
		val = gpio_pin_get_dt(input->sensor_gpio);
		if(input->config->polarity == 1){
			val = !val;
		}
	}

	return val;
}

void sensor_input_handle_event(struct sensor_input* input, uint64_t event_id){
	if(__builtin_bswap64(input->config->BE_event_sensor_on) == event_id){
		input->is_on = 1;
	}else if(__builtin_bswap64(input->config->BE_event_sensor_off) == event_id){
		input->is_on = 0;
	}
}

int sensor_input_valid(struct sensor_input* input){
	return input->config->sensor_enabled;
}

void switch_input_init(struct switch_input* input, struct gpio_dt_spec* gpio){
	input->switch_gpio = NULL;
	if(input->config->switch_input_type == 0){
		// board input
		input->switch_gpio = gpio;
	}
}

int switch_input_enabled(struct switch_input* input){
	return input->config->switch_enabled;
}

int switch_input_value(struct switch_input* input){
  if(input->switch_gpio){
      int val = gpio_pin_get_dt(input->switch_gpio);
      if(input->config->polarity == FLAG_POLARITY_ACTIVE_LOW){
        val = !val;
      }
      return val;
  }

  return input->current_pos;
}

void switch_input_handle_event(struct switch_input* input, uint64_t event_id){
	if(__builtin_bswap64(input->config->BE_event_switch_reversed) == event_id){
		input->current_pos = 1;
	}else if(__builtin_bswap64(input->config->BE_event_switch_normal) == event_id){
		input->current_pos = 0;
	}
}
