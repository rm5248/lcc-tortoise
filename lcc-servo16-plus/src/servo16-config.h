/*
 * servo16-config.h
 *
 *  Created on: Oct 18, 2025
 *      Author: robert
 */

#ifndef LCC_SERVO16_PLUS_SRC_SERVO16_CONFIG_H_
#define LCC_SERVO16_PLUS_SRC_SERVO16_CONFIG_H_

#include <stdint.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include "dcc-decode-stm32.h"
#include "dcc-decoder.h"

#include "servo16-output-state.h"
#include "configuration_flusher.h"

struct lcc_event_context;

enum LEDFunctions{
	LED_FUNC_OFF,
	LED_FUNC_STEADY_ON,
	LED_FUNC_PULSE_SLOW, // 1hz
	LED_FUNC_PULSE_MED, // 2hz
	LED_FUNC_PULSE_FAST, // 4hz
	LED_FUNC_FADE_UP,
	LED_FUNC_FADE_DOWN,
};

enum BoardType{
	BOARD_TYPE_SERVO,
	BOARD_TYPE_LED,
	BOARD_TYPE_SIGNAL_LED,
};

/**
 * Board Event information
 *
 * Read from EPROM.
 */
struct BoardEvent{
	uint64_t BE_event_id;
	uint16_t arg1;
	uint16_t arg2;
	uint8_t led_function;
	uint8_t reserved[3];
};
_Static_assert(sizeof(struct BoardEvent) == 16, "bad size for BoardEvent");

/**
 * Board Output configuration
 *
 * Read from EPROM
 */
struct BoardOutput{
	char description[32];

	// Servo specific settings
	uint16_t BE_min_pulse;
	uint16_t BE_max_pulse;
	uint8_t servo_speed;

	uint8_t default_startup_event;

	uint8_t reserved[26];
	struct BoardEvent events[6];
};
_Static_assert(sizeof(struct BoardOutput) == 160, "bad size for BoardOutput");

/**
 * Board configuration - space 253
 */
struct BoardConfig{
	uint8_t address;
	uint8_t board_type;
	uint8_t reserved[30];
};
_Static_assert(sizeof(struct BoardConfig) == 32, "bad size for BoardConfig");

struct ServoEvent{
	uint64_t BE_event_id;
	uint16_t BE_servo_position;
	char reserved[14];
};
_Static_assert(sizeof(struct ServoEvent) == 24, "bad size for ServoEvent");

/**
 * Servo configuration - one for each output
 */
struct ServoConfig{
	char servo_name[32];
	uint8_t servo_speed;
	uint8_t default_output;
	char reserved[30];
	struct ServoEvent events[6];
};
_Static_assert(sizeof(struct ServoConfig) == 208, "bad size for ServoConfig");

struct LEDEvent{
	uint64_t BE_event_id;
	uint16_t BE_argument1;
	uint16_t BE_argument2;
	uint8_t led_state;
	char reserved[11];
};
_Static_assert(sizeof(struct LEDEvent) == 24, "bad size for LEDEvent");

/**
 * LED configuration - one for each output
 */
struct LEDConfiguration{
	char servo_name[32];
	uint8_t default_output;
	char reserved[31];
	struct LEDEvent events[6];
};
_Static_assert(sizeof(struct LEDConfiguration) == 208, "bad size for LEDConfiguration");

struct Board{
	// Configuration that comes from EEPROM for this board
	struct BoardConfig* config;

	union {
		// If the board is set to do servos, the configuration
		struct ServoConfig* servo_config;
		// If the board is set to do LEDs, the configuration
		struct LEDConfiguration* led_config;
	};

	// Runtime state information
//	struct OutputState output_state[16];

	uint8_t ok;
};

struct Servo16PlusNameDescription {
	char node_name[64];
	char node_description[64];
};

#define NUM_EXPANSION_BOARDS 4

struct Servo16PlusState{
	// Segment 253
	struct BoardConfig pwm_boards_config[NUM_EXPANSION_BOARDS];
	// Segment 250
	struct ServoConfig servo_config[NUM_EXPANSION_BOARDS][16];
	// Segment 249
	struct LEDConfiguration led_config[NUM_EXPANSION_BOARDS][16];

	struct Board boards[4];
	const struct gpio_dt_spec oe_pin;
	const struct gpio_dt_spec green_led;
	const struct gpio_dt_spec blue_led;
	const struct gpio_dt_spec gold_led;
	const struct gpio_dt_spec blue_button;
	const struct gpio_dt_spec gold_button;

	// Segment 251
	struct Servo16PlusNameDescription name_desc;

	struct configuration_flusher config_flusher;

	// DCC Decoding variables
	struct dcc_decoder_stm32 dcc_decoder_stm32;
	uint32_t last_rx_dcc_message;


	struct {
		uint32_t lcc_offset;
		uint32_t segment_253_offset;
		uint32_t segment_251_offset;
		uint32_t segment_250_offset;
		uint32_t segment_249_offset;
	} eeprom_offsets;
};

extern struct Servo16PlusState servo16_state;

int init_state(uint64_t board_id);
void init_all_daughterboards();
void init_start_state();

void output_enable(void);

int save_config_to_flash();
int load_config_from_flash();
int save_globals_to_flash();
int load_global_config_from_flash();
void add_all_events_consumed(struct lcc_event_context* ctx);

#endif /* LCC_SERVO16_PLUS_SRC_SERVO16_CONFIG_H_ */
