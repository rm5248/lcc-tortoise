/*
 * dcc-decode.c
 *
 *  Created on: Jun 17, 2024
 *      Author: robert
 */
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <stm32g0xx_ll_tim.h>
#include <stm32g0xx_ll_gpio.h>

#include "dcc-decoder.h"
#include "dcc-packet-parser.h"

#include "dcc-decode-stm32.h"
#include "dcc-decoder.h"

static uint32_t readings[1000];
static uint32_t pos = 0;
static uint32_t num_sr = 0;
static uint32_t num_bad_bits = 0;

struct dcc_decoder_stm32 dcc_decode_ctx;

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
	ISR_DIRECT_HEADER();
	// Return 1 if a reschedule should be done, 0 if not
	int ret = 0;

	if (LL_TIM_IsActiveFlag_CC1(TIM2))
	{
		const uint32_t value = LL_TIM_IC_GetCaptureCH1(TIM2); // read value and clear the IRQ by reading CCR1
		// Timer should keep on running here.
		// We need to rebase the time on the counter such that 0 is the time that the value changed.
		// Since our IRQ doesn't happen immediately, calculate the new difference and set the timer
		// to that value.
		uint32_t new_value = LL_TIM_GetCounter(TIM2) - value;
		LL_TIM_SetCounter(TIM2, new_value);
		uint32_t sr = TIM2->SR;
		int overcap = sr & (0x01 << 9);
		if(overcap){
			num_sr++;
			sr &= ~(0x01 << 9);
			TIM2->SR = sr;
		}

		k_msgq_put(&dcc_decode_ctx.readings, &value, K_NO_WAIT);
		printf(":");

		// We don't need to reschedule on every bit change, but we do need to do
		// it on a somewhat regular schedule.
		if(k_msgq_num_used_get(&dcc_decode_ctx.readings) >= 16){
			ret = 1;
		}
	}

	ISR_DIRECT_FOOTER(ret);
	return ret;
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
	TIM2->CCMR1 = 1 | (0x2 << 4); // seems okay??
//	TIM2->CCMR1 = 1 | (0x3 << 4);
	// 3: set filter duration in CCMR1
	// 4: Set polarity of transitions.  We want both for DCC
	TIM2->CCER = 0xb; // both polarity
//	TIM2->CCER = 0x1; // rising only
	// 5: set input prescaler.  Leave at default values
	// 6: enable capture from counter into capture register
	// 7: enable the interrupt
	LL_TIM_EnableIT_CC1(TIM2);
	LL_TIM_EnableCounter(TIM2);

	return 0;
}

int dcc_decoder_init(struct dcc_decoder_stm32* decoder){
	k_msgq_init(&decoder->readings,
			decoder->readings_data,
			sizeof(uint32_t),
			ARRAY_SIZE(decoder->readings_data));

	timer2_init();

	// Configure the pin input for timer 2
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_15, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_15, LL_GPIO_PULL_DOWN);
	LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_15, LL_GPIO_AF_2);

	// Custom IRQ handler for timer 2
	IRQ_DIRECT_CONNECT(TIM2_IRQn, 0, tim2_irq_fn, 0);
	irq_enable(TIM2_IRQn);

	// Configure and start timer2
	int res = timer2_init();
	if(res != 0){
		printf("can't init: %d\n", res);
	}

	decoder->packet_parser = dcc_packet_parser_new();
	if(decoder->packet_parser == NULL){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return -1;
	}

	decoder->dcc_decoder = dcc_decoder_new(DCC_DECODER_IRQ_BOTH, DCC_DECODER_FLAG_EXPAND_ONE_BIT_DURATION);
	if(decoder->dcc_decoder == NULL){
		// This uses only static data to initialize, so this should never happen.
		printf("error: unable to initialize! %d\n", __LINE__);
		return -1;
	}

	return 0;
}

void dcc_decoder_disable(){
	irq_disable(TIM2_IRQn);
}
