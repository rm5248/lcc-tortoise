/*
 * servo16-state.c
 *
 *  Created on: Oct 23, 2025
 *      Author: robert
 */

#include "servo16-config.h"
#include "lcc-event.h"

#include <zephyr/storage/flash_map.h>

struct Servo16PlusState servo16_state = {
		.boards = {
				{
						.pwm_outputs = {
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 0),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 2),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 4),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 6),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 8),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 10),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 12),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 14),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 16),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 18),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 20),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 22),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 24),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 26),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 28),
								PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(builtin), 30),
						}
				}
		},
		.oe_pin = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), oe_gpios),
		.green_led = GPIO_DT_SPEC_GET(DT_ALIAS(greenled), gpios),
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

int init_state(uint64_t board_id){
	uint64_t start_event_id = board_id << 16;

	for(int x = 0; x < 4; x++){
		servo16_state.boards[x].config = &servo16_state.pwm_boards_config[x];

		for(int y = 0; y < 16; y++){
			servo16_state.pwm_boards_config[x].outputs[y].BE_min_pulse = __builtin_bswap16(1000);
			servo16_state.pwm_boards_config[x].outputs[y].BE_max_pulse = __builtin_bswap16(2000);
			for(int z = 0; z < 6; z++){
				servo16_state.pwm_boards_config[x].outputs[y].events[z].BE_event_id = __builtin_bswap64(start_event_id);
				start_event_id++;
			}
		}
	}

	servo16_state.boards[1].config = &servo16_state.pwm_boards_config[1];
	servo16_state.boards[2].config = &servo16_state.pwm_boards_config[2];
	servo16_state.boards[3].config = &servo16_state.pwm_boards_config[3];


	gpio_pin_configure_dt(&servo16_state.oe_pin, GPIO_OUTPUT_INACTIVE);

	if(init_led(&servo16_state.green_led) < 0){
		return -1;
	}

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
