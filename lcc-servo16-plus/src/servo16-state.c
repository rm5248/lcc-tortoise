/*
 * servo16-state.c
 *
 *  Created on: Oct 23, 2025
 *      Author: robert
 */

#include "servo16-config.h"
#include "lcc-event.h"
#include "servo16-output-state.h"

#include <zephyr/storage/flash_map.h>

struct Servo16PlusState servo16_state = {
		.boards = {
				{
						.config = NULL,
						.output_state = {
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 0) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 2) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 4) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 6) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 8) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 10) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 12) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 14) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 16) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 18) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 20) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 22) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 24) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 26) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 28) },
								{ .pwm_output = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 30) }
						}
				}
		},
		.oe_pin = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), oe_gpios),
		.green_led = GPIO_DT_SPEC_GET(DT_ALIAS(greenled), gpios),
		.blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(blueled), gpios),
		.gold_led = GPIO_DT_SPEC_GET(DT_ALIAS(goldled), gpios),
		.blue_button = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_switch), gpios),
		.gold_button = GPIO_DT_SPEC_GET(DT_NODELABEL(gold_switch), gpios),
};

static int init_led(const struct gpio_dt_spec* led){
	if (!gpio_is_ready_dt(led)) {
		return -1;
	}

	if(gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE) < 0){
		return -1;
	}

	return 0;
}

static int init_button(const struct gpio_dt_spec* button){
	if (!gpio_is_ready_dt(button)) {
		return -1;
	}

	if(gpio_pin_configure_dt(button, GPIO_INPUT) < 0){
		return -1;
	}

	if(gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_BOTH) < 0){
		return -1;
	}

	// TODO callbacks on button press

	return 0;
}

int init_state(uint64_t board_id){
	uint64_t start_event_id = board_id << 16;

	servo16_state.last_rx_dcc_message = 0;

	// Initialize the default values for all the boards we can control.
	// This will likely get overwritten by values from flash
	for(int x = 0; x < 4; x++){
		struct BoardConfig* board_config = &servo16_state.pwm_boards_config[x];;
		servo16_state.boards[x].config = board_config;

		// Initialize default values for the board config
		for(int y = 0; y < 16; y++){
			board_config->outputs[y].BE_min_pulse = __builtin_bswap16(1000);
			board_config->outputs[y].BE_max_pulse = __builtin_bswap16(2000);
			for(int z = 0; z < 6; z++){
				board_config->outputs[y].events[z].BE_event_id = __builtin_bswap64(start_event_id);
				start_event_id++;
			}
		}

		// Initialize the default values for the output state
		for(int z = 0; z < 16; z++){
			struct OutputState* output_state = &servo16_state.boards[x].output_state[z];
			struct BoardOutput* output_config = &board_config->outputs[z];

			output_state_init(output_state, output_config);
		}
	}

	servo16_state.pwm_boards_config[0].address = 0x40;
	servo16_state.pwm_boards_config[1].address = 0x41;
	servo16_state.pwm_boards_config[2].address = 0x42;
	servo16_state.pwm_boards_config[3].address = 0x43;

	gpio_pin_configure_dt(&servo16_state.oe_pin, GPIO_OUTPUT_INACTIVE);

	if(init_led(&servo16_state.green_led) < 0){
		return -1;
	}
	if(init_led(&servo16_state.blue_led) < 0){
		return -1;
	}
	if(init_led(&servo16_state.gold_led) < 0){
		return -1;
	}
//	if(init_button(&servo16_state.blue_button) < 0){
//		return -1;
//	}
//	if(init_button(&servo16_state.gold_button) < 0){
//		return -1;
//	}

	return 0;
}

void output_enable(){
	gpio_pin_set_dt(&servo16_state.oe_pin, 1);
}

int save_config_to_flash(){
	int ret = 0;
	const struct flash_area* storage_area = NULL;
	int id = FIXED_PARTITION_ID(config_partition);

	if(flash_area_open(id, &storage_area) < 0){
		return 1;
	}

	ret = flash_area_erase(storage_area, 0, sizeof(servo16_state.pwm_boards_config));
	if(ret){
		goto out;
	}

	int err = flash_area_write(storage_area, 0, servo16_state.pwm_boards_config, sizeof(servo16_state.pwm_boards_config));
	if( err < 0){
		ret = -1;
	}

out:
	if(ret){
		printf("Unable to save configs to flash: %d\n", err);
	}
	flash_area_close(storage_area);

//	powerhandle_check_if_save_required();
	return ret;
}

int load_config_from_flash(){
	int ret = 0;
	const struct flash_area* storage_area = NULL;
	uint8_t buffer[8];
	int id = FIXED_PARTITION_ID(config_partition);

	if(flash_area_open(id, &storage_area) < 0){
		return 1;
	}

	if(flash_area_read(storage_area, 0, buffer, sizeof(buffer)) < 0){
		ret = -1;
		goto out;
	}

	// Check to see if the first 8 bytes are 0xFF.  If so, we assume the data is uninitialized
	const uint8_t mem[8] = {
			0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF
	};
	if(memcmp(buffer, mem, 8) == 0){
		ret = 2;
		goto out;
	}

	if(flash_area_read(storage_area, 0, servo16_state.pwm_boards_config, sizeof(servo16_state.pwm_boards_config)) < 0){
		goto out;
	}

out:
	flash_area_close(storage_area);
	return ret;
}

void add_all_events_consumed(struct lcc_event_context* evt_ctx){
	lcc_event_clear_events(evt_ctx, LCC_EVENT_CONTEXT_CLEAR_EVENTS_CONSUMED);
	lcc_event_add_event_consumed_transaction_start(evt_ctx);

	for(int x = 0; x < 4; x++){
		for(int y = 0; y < 16; y++){
			for(int z = 0; z < 6; z++){
				uint64_t event_id = __builtin_bswap64(servo16_state.pwm_boards_config[x].outputs[y].events[z].BE_event_id);
				lcc_event_add_event_consumed(evt_ctx, event_id);
			}
		}
	}

	lcc_event_add_event_consumed_transaction_end(evt_ctx);
}
