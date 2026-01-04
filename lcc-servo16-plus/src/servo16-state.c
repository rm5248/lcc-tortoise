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
#include <zephyr/drivers/i2c.h>

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

struct Servo16PlusGlobalConfig servo16_global = {0};

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

int save_globals_to_flash(){
	int ret = 0;
	const struct flash_area* storage_area = NULL;
	int id = FIXED_PARTITION_ID(global_partition);

	if(flash_area_open(id, &storage_area) < 0){
		return 1;
	}

	ret = flash_area_erase(storage_area, 0, sizeof(servo16_global));
	if(ret){
		goto out;
	}

	int err = flash_area_write(storage_area, 0, &servo16_global, sizeof(servo16_global));
	if( err < 0){
		ret = -1;
	}

out:
	if(ret){
		printf("Unable to save global configs to flash: %d\n", err);
	}
	flash_area_close(storage_area);

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

int load_global_config_from_flash(){
	int ret = 0;
	const struct flash_area* storage_area = NULL;
	uint8_t buffer[8];
	int id = FIXED_PARTITION_ID(global_partition);

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

	if(flash_area_read(storage_area, 0, &servo16_global, sizeof(servo16_global)) < 0){
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

static int check_for_daughterboard(int address){
	const struct device* i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));
	uint8_t buf;

	if(i2c_read(i2c_dev, &buf, 1, address) < 0){
		return 0;
	}

	return 1;
}

void init_all_daughterboards(){
	static const struct pwm_dt_spec pwm_output_node_0x41[] = {
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 0),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 2),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 4),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 6),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 8),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 10),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 12),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 14),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 16),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 18),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 20),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 22),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 24),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 26),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 28),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node1), 30)
	};
	static const struct device* device_0x41 = DEVICE_DT_GET(DT_NODELABEL(pca9685_node1));

	static const struct pwm_dt_spec pwm_output_node_0x42[] = {
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 0),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 2),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 4),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 6),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 8),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 10),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 12),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 14),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 16),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 18),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 20),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 22),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 24),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 26),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 28),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node2), 30)
	};
	static const struct device* device_0x42 = DEVICE_DT_GET(DT_NODELABEL(pca9685_node2));

	static const struct pwm_dt_spec pwm_output_node_0x43[] = {
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 0),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 2),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 4),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 6),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 8),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 10),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 12),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 14),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 16),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 18),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 20),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 22),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 24),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 26),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 28),
			PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pwm_node3), 30)
	};
	static const struct device* device_0x43 = DEVICE_DT_GET(DT_NODELABEL(pca9685_node3));

	if(check_for_daughterboard(0x40) == 0){
		printf("ERROR: Builtin board not found??\n");
	}
	for(int board = 0; board < 4; board++){
		int addr = servo16_state.pwm_boards_config[board].address;
		int found = check_for_daughterboard(addr);
		const struct pwm_dt_spec* base;
		const struct device* dev;
		printf("Looking for board 0x%02X ... %s\n", addr, found ? "found" : "not found");

		servo16_state.boards[board].ok = !!found;

		if(addr == 0x41){
			base = pwm_output_node_0x41;
			dev = device_0x41;
		}else if(addr == 0x42){
			base = pwm_output_node_0x42;
			dev = device_0x42;
		}else if(addr == 0x43){
			base = pwm_output_node_0x43;
			dev = device_0x43;
		}else{
			if(addr != 0x40){
				printf("Address 0x%02X currently invalid, ignoring...\n", addr);
			}
			continue;
		}

		if(addr != 0x40 && found){
			// Initialize the driver
			int ret = device_init(dev);
			if(ret){
				printf("Unable to init device\n");
				continue;
			}

			for(int output = 0; output < 16; output++){
				struct OutputState* out_state = &servo16_state.boards[board].output_state[output];
				memcpy(&out_state->pwm_output,
						&base[output],
						sizeof(struct pwm_dt_spec));
			}
		}
	}
}

void init_start_state(){
	for(int board = 0; board < 4; board++){
		if(!servo16_state.boards[board].ok){
			continue;
		}

		uint8_t board_type = servo16_state.boards[board].config->board_type;
		for(int output = 0; output < 16; output++){
			struct OutputState* out_state = &servo16_state.boards[board].output_state[output];
			uint8_t default_output = out_state->output_config->default_startup_event;

			output_state_perform_initial(out_state, board_type, default_output);
		}
	}
}
