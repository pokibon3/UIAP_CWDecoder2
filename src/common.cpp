//
// CW Decoder Common functions
//
#include <stdint.h>
#include "common.h"
#include "ch32fun.h"

#if defined(BOARD_CH32V006)
static const uint8_t ADC_LEVEL_SHIFT_FFT_V006 = 1;

#ifndef TIM_OC1M_2
#define TIM_OC1M_2 ((uint16_t)0x0040)
#endif
#ifndef TIM_OC1M_1
#define TIM_OC1M_1 ((uint16_t)0x0020)
#endif
#ifndef TIM_OC1PE
#define TIM_OC1PE ((uint16_t)0x0008)
#endif
#ifndef TIM_ARPE
#define TIM_ARPE ((uint16_t)0x0080)
#endif
#ifndef TIM_CC1E
#define TIM_CC1E ((uint16_t)0x0001)
#endif
#ifndef TIM_UG
#define TIM_UG ((uint16_t)0x0001)
#endif
#ifndef TIM_MOE
#define TIM_MOE ((uint16_t)0x8000)
#endif
#ifndef TIM_CEN
#define TIM_CEN ((uint16_t)0x0001)
#endif
#ifndef TIM_MMS
#define TIM_MMS ((uint16_t)0x0070)
#endif
#ifndef TIM_MMS_1
#define TIM_MMS_1 ((uint16_t)0x0020)
#endif

enum {
	ADC_MODE_V006_IDLE = 0,
	ADC_MODE_V006_CW = 1,
	ADC_MODE_V006_FFT = 2,
};

static volatile uint8_t adc_mode_v006 = ADC_MODE_V006_IDLE;
static volatile uint16_t adc_fifo_v006[256];
static volatile uint16_t adc_fifo_wr_v006 = 0;
static volatile uint16_t adc_fifo_rd_v006 = 0;
static volatile uint32_t adc_irq_count_v006 = 0;
#endif

// Pin mapping (UIAP board)
static const uint8_t SW1_PIN = 1; // PA1
static const uint8_t SW2_PIN = 4; // PC4
static const uint8_t SW3_PIN = 2; // PD2
static const uint8_t ADC_PIN = 2; // PA2 (ADC_IN0)
#if defined(BOARD_CH32V006)
static const uint8_t ADC_CH_A2 = 2; // PA2 = ADC_IN2 on CH32V006
#else
static const uint8_t LED_PIN = 0; // PC0
static const uint8_t ADC_CH_A2 = 0; // PA2 = ADC_IN0 on CH32V003
#endif
static const uint8_t UART_PIN = 5; // PD5
static const uint8_t TEST_PIN = 6; // PD6

// GPIO CFGLR nibble encodings (MODE[1:0] + CNF[1:0] << 2)
static const uint8_t GPIO_CFG_INPUT_ANALOG = 0x0;
static const uint8_t GPIO_CFG_INPUT_PUPD = 0x8;
static const uint8_t GPIO_CFG_OUTPUT_PP_10M = 0x1;
static const uint8_t GPIO_CFG_OUTPUT_AF_PP_10M = 0x9;

static inline void gpio_cfg_pin(GPIO_TypeDef *port, uint8_t pin, uint8_t cfg)
{
	uint32_t shift = (uint32_t)pin * 4U;
	uint32_t mask = (uint32_t)0xFU << shift;
	port->CFGLR = (port->CFGLR & ~mask) | ((uint32_t)cfg << shift);
}

static inline void gpio_write(GPIO_TypeDef *port, uint8_t pin, uint8_t level)
{
	if (level == GPIO_HIGH) {
		port->BSHR = (uint32_t)1U << pin;
	} else {
		port->BCR = (uint32_t)1U << pin;
	}
}

static inline uint8_t gpio_read(GPIO_TypeDef *port, uint8_t pin)
{
	return (uint8_t)((port->INDR >> pin) & 1U);
}

static void adc_init_ch0(void)
{
	RCC->APB2PCENR |= RCC_APB2Periph_ADC1;
	RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_ADC1;

#if defined(BOARD_CH32V006)
	// Keep 48 MHz system clock and follow the x00x/x035 polled-ADC startup order.
	ADC1->CTLR2 |= ADC_ADON;
	ADC1->RSQR1 = 0;
	ADC1->RSQR2 = 0;
#endif

	// Sample time: medium (15 cycles) for all channels.
	ADC1->SAMPTR2 = (ADC_SMP0_1 << (3 * 0)) | (ADC_SMP0_1 << (3 * 1)) |
		(ADC_SMP0_1 << (3 * 2)) | (ADC_SMP0_1 << (3 * 3)) |
		(ADC_SMP0_1 << (3 * 4)) | (ADC_SMP0_1 << (3 * 5)) |
		(ADC_SMP0_1 << (3 * 6)) | (ADC_SMP0_1 << (3 * 7)) |
		(ADC_SMP0_1 << (3 * 8)) | (ADC_SMP0_1 << (3 * 9));
	ADC1->SAMPTR1 = (ADC_SMP0_1 << (3 * 0)) | (ADC_SMP0_1 << (3 * 1)) |
		(ADC_SMP0_1 << (3 * 2)) | (ADC_SMP0_1 << (3 * 3)) |
		(ADC_SMP0_1 << (3 * 4)) | (ADC_SMP0_1 << (3 * 5));

#if defined(BOARD_CH32V006)
	// Default CH32V006 startup uses polled ADC mode.
	ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL;
#else
	ADC1->CTLR2 |= ADC_ADON | ADC_EXTTRIG;
#endif
	ADC1->CTLR2 |= CTLR2_RSTCAL_Set;
	while (ADC1->CTLR2 & CTLR2_RSTCAL_Set) {
	}
	ADC1->CTLR2 |= CTLR2_CAL_Set;
	while (ADC1->CTLR2 & CTLR2_CAL_Set) {
	}
#if defined(BOARD_CH32V006)
	ADC1->CTLR1 &= ~ADC_EOCIE;
#endif
}

static inline uint16_t adc_read_ch0_raw()
{
	ADC1->RSQR3 = ADC_CH_A2;
	Delay_Us(GPIO_ADC_MUX_DELAY);
	ADC1->CTLR2 |= ADC_SWSTART;
#if defined(BOARD_CH32V006)
	uint32_t start = micros();
	while (!(ADC1->STATR & ADC_EOC)) {
		if ((micros() - start) > 1000U) {
			return (uint16_t)ADC1->RDATAR;
		}
	}
#else
	while (!(ADC1->STATR & ADC_EOC)) {
	}
#endif
	return (uint16_t)ADC1->RDATAR;
}

#if defined(BOARD_CH32V006)
static void adc_fifo_reset_v006(void)
{
	adc_fifo_wr_v006 = 0;
	adc_fifo_rd_v006 = 0;
}

static uint16_t tim1_arr_from_sampling_period(uint16_t sample_period_us)
{
	uint32_t ticks = ((uint32_t)FUNCONF_SYSTEM_CORE_CLOCK * (uint32_t)sample_period_us + 450000U) / 900000U;
	if (ticks < 2U) {
		ticks = 2U;
	}
	return (uint16_t)(ticks - 1U);
}

void adc_set_cw_mode(void)
{
	adc_mode_v006 = ADC_MODE_V006_CW;
	adc_fifo_reset_v006();
	ADC1->CTLR1 &= ~ADC_EOCIE;
	ADC1->RSQR1 = 0;
	ADC1->RSQR2 = 0;
	ADC1->RSQR3 = ADC_CH_A2;
	ADC1->CTLR2 = ADC_ADON | ADC_EXTTRIG | ADC_ExternalTrigConv_T1_TRGO;
	ADC1->STATR = 0;
	ADC1->CTLR1 |= ADC_EOCIE;
	NVIC->IPRIOR[ADC_IRQn] = 0 << 7 | 1 << 6;
	NVIC->IENR[((uint32_t)(ADC_IRQn) >> 5)] |= (1U << ((uint32_t)(ADC_IRQn) & 0x1F));
}

void adc_set_polled_mode(void)
{
	adc_mode_v006 = ADC_MODE_V006_FFT;
	adc_fifo_reset_v006();
	ADC1->CTLR1 &= ~ADC_EOCIE;
	ADC1->RSQR1 = 0;
	ADC1->RSQR2 = 0;
	ADC1->RSQR3 = ADC_CH_A2;
	ADC1->CTLR2 = ADC_ADON | ADC_EXTTRIG | ADC_ExternalTrigConv_T1_TRGO;
	ADC1->STATR = 0;
	ADC1->CTLR1 |= ADC_EOCIE;
	NVIC->IPRIOR[ADC_IRQn] = 0 << 7 | 1 << 6;
	NVIC->IENR[((uint32_t)(ADC_IRQn) >> 5)] |= (1U << ((uint32_t)(ADC_IRQn) & 0x1F));
}

uint8_t adc_get_mode_v006(void)
{
	return adc_mode_v006;
}

void adc_push_sample_v006(uint16_t sample)
{
	adc_irq_count_v006++;
	uint16_t next = (uint16_t)((adc_fifo_wr_v006 + 1U) & 0xFFU);
	if (next == adc_fifo_rd_v006) {
		adc_fifo_rd_v006 = (uint16_t)((adc_fifo_rd_v006 + 1U) & 0xFFU);
	}
	adc_fifo_v006[adc_fifo_wr_v006] = sample;
	adc_fifo_wr_v006 = next;
}

uint32_t adc_get_irq_count_v006(void)
{
	return adc_irq_count_v006;
}

void adc_reset_irq_count_v006(void)
{
	adc_irq_count_v006 = 0;
}

int adc_pop_u8_v006(int8_t *dst, uint16_t samples, uint16_t *average)
{
	uint32_t sum = 0;
	if (!dst || !average || samples == 0) {
		return 0;
	}

	for (uint16_t i = 0; i < samples; i++) {
		uint32_t start = micros();
		while (adc_fifo_rd_v006 == adc_fifo_wr_v006) {
			if ((micros() - start) > 2000U) {
				return 0;
			}
		}
		uint16_t raw = adc_fifo_v006[adc_fifo_rd_v006];
		adc_fifo_rd_v006 = (uint16_t)((adc_fifo_rd_v006 + 1U) & 0xFFU);
		uint8_t val = (uint8_t)((raw >> (2 + ADC_LEVEL_SHIFT_FFT_V006)) & 0xFF);
		sum += val;
		dst[i] = (int8_t)val;
	}

	*average = (uint16_t)(sum / samples);
	return 1;
}
#endif

void tim1_pwm_init(void)
{
	RCC->APB2PCENR |= RCC_APB2Periph_TIM1;
	RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;

	TIM1->PSC = 0;
#if defined(BOARD_CH32V006)
	TIM1->ATRLR = tim1_arr_from_sampling_period(sampling_period_us);
#else
	TIM1->ATRLR = 5859 - 1;
#endif
	TIM1->CHCTLR1 |= TIM_OC1M_2 | TIM_OC1M_1 | TIM_OC1PE;
	TIM1->CTLR1 |= TIM_ARPE;
	TIM1->CCER |= TIM_CC1E;
	TIM1->CTLR2 = (TIM1->CTLR2 & ~TIM_MMS) | TIM_MMS_1;
	TIM1->SWEVGR |= TIM_UG;
	TIM1->INTFR = (uint16_t)~TIM_IT_Update;
#if !defined(BOARD_CH32V006)
	NVIC->IPRIOR[TIM1_UP_IRQn] = 0 << 7 | 1 << 6;
	NVIC->IENR[((uint32_t)(TIM1_UP_IRQn) >> 5)] |= (1U << ((uint32_t)(TIM1_UP_IRQn) & 0x1F));
	TIM1->DMAINTENR |= TIM_IT_Update;
#endif
	TIM1->CH1CVR = 128;
	TIM1->BDTR |= TIM_MOE;
	TIM1->CTLR1 |= TIM_CEN;
}

void tim1_pwm_stop(void)
{
	TIM1->DMAINTENR &= ~TIM_IT_Update;
	TIM1->CTLR1 &= ~TIM_CEN;
}

//==================================================================
// setup
//==================================================================
int GPIO_setup()
{
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC |
		RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO;

	// Switches: input pull-up.
	gpio_cfg_pin(GPIOA, SW1_PIN, GPIO_CFG_INPUT_PUPD);
	gpio_cfg_pin(GPIOC, SW2_PIN, GPIO_CFG_INPUT_PUPD);
	gpio_cfg_pin(GPIOD, SW3_PIN, GPIO_CFG_INPUT_PUPD);
	gpio_write(GPIOA, SW1_PIN, GPIO_HIGH);
	gpio_write(GPIOC, SW2_PIN, GPIO_HIGH);
	gpio_write(GPIOD, SW3_PIN, GPIO_HIGH);

	// ADC input: analog.
	gpio_cfg_pin(GPIOA, ADC_PIN, GPIO_CFG_INPUT_ANALOG);

	// TEST: output push-pull.
	gpio_cfg_pin(GPIOD, TEST_PIN, GPIO_CFG_OUTPUT_PP_10M);
	gpio_write(GPIOD, TEST_PIN, GPIO_LOW);

#if !defined(BOARD_CH32V006)
	// Keep CH32V003 behavior exactly as before.
	gpio_cfg_pin(GPIOC, LED_PIN, GPIO_CFG_OUTPUT_PP_10M);
	gpio_write(GPIOC, LED_PIN, GPIO_LOW);
#endif

	// UART TX pin: alternate function push-pull.
	gpio_cfg_pin(GPIOD, UART_PIN, GPIO_CFG_OUTPUT_AF_PP_10M);

	adc_init_ch0();
	return 0;
}

//==================================================================
// check switch
//==================================================================
int check_input()
{
	if (!gpio_read(GPIOA, SW1_PIN)) {
		return 1;
	}
	if (!gpio_read(GPIOD, SW3_PIN)) {
		return 3;
	}
	if (!gpio_read(GPIOC, SW2_PIN)) {
		return 2;
	}
	return 0;
}

uint16_t adc_read_raw()
{
	return adc_read_ch0_raw();
}

uint16_t adc_capture_u8(int8_t *dst, uint16_t samples, uint16_t sample_period_us)
{
	uint32_t sum = 0;
	if (!dst || samples == 0) {
		return 0;
	}

#if defined(BOARD_CH32V006)
	(void)sample_period_us;
	uint16_t average = 0;
	if (!adc_pop_u8_v006(dst, samples, &average)) {
		return 0;
	}
	return average;
#else
	for (uint16_t i = 0; i < samples; i++) {
		uint32_t t = micros();
		uint8_t val = (uint8_t)((adc_read_ch0_raw() >> 2) & 0xFF);
		sum += val;
		dst[i] = (int8_t)val;
		while ((micros() - t) < sample_period_us) {
		}
	}
	return (uint16_t)(sum / samples);
#endif
}

void gpio_write_led(uint8_t level)
{
#if !defined(BOARD_CH32V006)
	gpio_write(GPIOC, LED_PIN, level);
#else
	(void)level;
#endif
}

void gpio_write_test(uint8_t level)
{
	gpio_write(GPIOD, TEST_PIN, level);
}
