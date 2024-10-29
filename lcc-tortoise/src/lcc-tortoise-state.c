/*
 * lcc-tortoise-state.c
 *
 *  Created on: May 19, 2024
 *      Author: robert
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/devicetree.h>
#include <stm32g0xx_ll_tim.h>
#include <stm32g0xx_ll_gpio.h>

#include "lcc-tortoise-state.h"
#include "tortoise.h"
#include "dcc-decoder.h"

struct lcc_tortoise_state lcc_tortoise_state = {
		.green_led = GPIO_DT_SPEC_GET(DT_ALIAS(greenled), gpios),
		.blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(blueled), gpios),
		.gold_led = GPIO_DT_SPEC_GET(DT_ALIAS(goldled), gpios),
		.blue_button = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_switch), gpios),
		.gold_button = GPIO_DT_SPEC_GET(DT_NODELABEL(gold_switch), gpios),
		.tortoises = {
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise0), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise1), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise1), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise2), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise2), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise3), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise3), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise4), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise4), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise5), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise5), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise6), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise6), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
				{
						.gpios = {
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise7), gpios, 0),
								GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(tortoise7), gpios, 1)
						},
						.current_position = POSITION_REVERSE
				},
		}
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

	// TODO callbacks on button press

	return 0;
}

uint32_t readings[1000];
uint32_t pos = 0;
uint32_t num_sr = 0;

static void debug_isr_halfbit(uint32_t cycle_time_usec){
	readings[pos++] = cycle_time_usec;
	if(pos == (sizeof(readings)/sizeof(readings[0]))){
		pos = 0;
//		printf("readings:\n");
		int num_readings_good = 0;
		int num_readings_bad = 0;
		for(int x = 0; x < sizeof(readings)/sizeof(readings[0]); x++){
			printf("%lu,", readings[x]);
			if(readings[x] >= 52 && readings[x] <= 64){
				num_readings_good++;
//				printf("1");
			}else if(readings[x] >= 90 && readings[x] <= 200){
				num_readings_good++;
//				printf("0");
			}else{
				num_readings_bad++;
//				printf("-");
			}
		}
		printf("good: %d bad: %d over: %d\n", num_readings_good, num_readings_bad, num_sr);
		num_sr = 0;
	}
}

static void debug_isr_fullbit(uint32_t cycle_time_usec){
	readings[pos++] = cycle_time_usec;
	if(pos == (sizeof(readings)/sizeof(readings[0]))){
		pos = 0;
//		printf("readings:\n");
		int num_readings_good = 0;
		int num_readings_bad = 0;
		for(int x = 0; x < sizeof(readings)/sizeof(readings[0]); x++){
			printf("%lu,", readings[x]);
			if(readings[x] >= (52*2) && readings[x] <= (64*2)){
				num_readings_good++;
//				printf("1");
			}else if(readings[x] >= (90*2) && readings[x] <= (200*2)){
				num_readings_good++;
//				printf("0");
			}else{
				num_readings_bad++;
//				printf("-");
			}
		}
		printf("good: %d bad: %d over: %d\n", num_readings_good, num_readings_bad, num_sr);
		num_sr = 0;
	}
}

ISR_DIRECT_DECLARE(tim2_irq_fn)
{
	if (LL_TIM_IsActiveFlag_CC1(TIM2))
	{
		uint32_t value = LL_TIM_IC_GetCaptureCH1(TIM2); // read value and clear the IRQ by reading CCR1
		// Timer should keep on running here.
		// We need to rebase the time on the counter such that 0 is the time that the value changed.
		// Since our IRQ doesn't happen immediately, calculate the new difference and set the timer
		// to that value.
		uint32_t new_value = LL_TIM_GetCounter(TIM2) - value;
		LL_TIM_SetCounter(TIM2, new_value);
		dcc_decoder_polarity_changed(lcc_tortoise_state.dcc_decoder, value);
		uint32_t sr = TIM2->SR;
		int overcap = sr & (0x01 << 9);
		if(overcap){
			num_sr++;
			sr &= ~(0x01 << 9);
			TIM2->SR = sr;
		}
//		debug_isr_halfbit(value);
	}

	return 0;
}

static int timer2_init(){
	// First enable the clock to the timer
	__HAL_RCC_TIM2_CLK_ENABLE();

	k_sleep(K_MSEC(1));

	// Set prescaler properly so we count every 1us
	LL_TIM_SetPrescaler(TIM2, 32);

	TIM2->CR1 = 0;
	TIM2->EGR = TIM_EGR_UG;

	// Steps are from RM0444, section 22.3.5
	// Step 1: Set TI1SEL bits to 0
	TIM2->TISEL = 0;
	// 2: select active input: TIM2_CCR1 must be linked to T1 input
	TIM2->CCMR1 = 1;
	// 3: set filter duration
	// (nothing to do, reset value at 0)
	// 4: Set polarity of transitions.  We want both for DCC
	TIM2->CCER = 0xb;
	// 5: set input prescaler.  Leave at default values
	// 6: enable capture from counter into capture register
	// 7: enable the interrupt
	LL_TIM_EnableIT_CC1(TIM2);
	LL_TIM_EnableCounter(TIM2);

	return 0;
}

int lcc_tortoise_state_init(){
	int gpio_int_type = GPIO_INT_EDGE_BOTH;

	if(init_led(&lcc_tortoise_state.green_led) < 0){
		return -1;
	}
	if(init_led(&lcc_tortoise_state.gold_led) < 0){
		return -1;
	}
	if(init_led(&lcc_tortoise_state.blue_led) < 0){
		return -1;
	}
	if(init_button(&lcc_tortoise_state.blue_button) < 0){
		return -1;
	}
	if(init_button(&lcc_tortoise_state.gold_button) < 0){
		return -1;
	}

	{
		// Configure the pin input for timer 2
		LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_15, LL_GPIO_MODE_ALTERNATE);
		LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_15, LL_GPIO_AF_2);
	}

	// Custom IRQ handler for timer 2
	IRQ_DIRECT_CONNECT(TIM2_IRQn, 0, tim2_irq_fn, 0);
	irq_enable(TIM2_IRQn);

	// Configure and start timer2
	int res = timer2_init();
	if(res != 0){
		printf("can't init: %d\n", res);
	}

	lcc_tortoise_state.gpio_expander = DEVICE_DT_GET(DT_NODELABEL(gpio_expander));

	for(int x = 0; x < 8; x++){
		lcc_tortoise_state.tortoises[x].config = &lcc_tortoise_state.tortoise_config[x];

		if(tortoise_init(&lcc_tortoise_state.tortoises[x]) < 0){
			return -1;
		}
	}

	return 0;
}
