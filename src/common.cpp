//
//	CW Decoder Common functions
//
#include <stdint.h>
#include "common.h"
#include "ch32v003_GPIO_branchless.h"

static const uint8_t SW1_PIN = GPIOv_from_PORT_PIN(GPIO_port_A, 1); // for uiap
static const uint8_t SW2_PIN = GPIOv_from_PORT_PIN(GPIO_port_C, 4); // for uiap
static const uint8_t SW3_PIN = GPIOv_from_PORT_PIN(GPIO_port_D, 2);
static const uint8_t ADC_PIN = GPIOv_from_PORT_PIN(GPIO_port_A, 2); // for uiap
static const uint8_t LED_PIN = GPIOv_from_PORT_PIN(GPIO_port_C, 0); // for uiap
static const uint8_t UART_PIN = GPIOv_from_PORT_PIN(GPIO_port_D, 5);
static const uint8_t TEST_PIN = GPIOv_from_PORT_PIN(GPIO_port_D, 6);
void tim1_pwm_init( void )
{
	// Enable GPIOD and TIM1
	//RCC->APB2PCENR |= RCC_APB2Periph_GPIOD;
	RCC->APB2PCENR |= RCC_APB2Periph_TIM1;

	// PD2 is T1CH1, 10MHz Output alt func, push-pull (no remap)
	//GPIOD->CFGLR &= ~(0xf<<(4*2));
	//GPIOD->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF)<<(4*2);

	// Reset TIM1 to init all regs
	RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;

	// SMCFGR: default clk input is CK_INT
	// set TIM1 update rate ~8192Hz (48MHz / (0+1) / (5859) = 8192.7Hz)
	TIM1->PSC = 0;
	TIM1->ATRLR = 5859 - 1;

	// for channel 1, let CCxS stay 00 (output), set OCxM to 110 (PWM I)
	// enabling preload causes the new pulse width in compare capture register only to come into effect when UG bit in SWEVGR is set (= initiate update) (auto-clears)
	TIM1->CHCTLR1 |= TIM_OC1M_2 | TIM_OC1M_1 | TIM_OC1PE;

	// CTLR1: default is up, events generated, edge align
	// enable auto-reload of preload
	TIM1->CTLR1 |= TIM_ARPE;

	// Enable Channel output, set default state
	TIM1->CCER |= TIM_CC1E;

	// initialize counter
	TIM1->SWEVGR |= TIM_UG;
	// enable update interrupt
	TIM1->INTFR = (uint16_t)~TIM_IT_Update;
	NVIC->IPRIOR[TIM1_UP_IRQn] = 0 << 7 | 1 << 6;
	NVIC->IENR[((uint32_t)(TIM1_UP_IRQn) >> 5)] |= (1 << ((uint32_t)(TIM1_UP_IRQn) & 0x1F));
	TIM1->DMAINTENR |= TIM_IT_Update;
	// set default duty cycle 50% for channel 1
	TIM1->CH1CVR = 128;
	// Enable TIM1 main output
	TIM1->BDTR |= TIM_MOE;
	// Enable TIM1
	TIM1->CTLR1 |= TIM_CEN;
}

void tim1_pwm_stop(void)
{
	// Disable update interrupt and stop timer to avoid stray ISR use.
	TIM1->DMAINTENR &= ~TIM_IT_Update;
	TIM1->CTLR1 &= ~TIM_CEN;
}

//==================================================================
//	setup
//==================================================================
int GPIO_setup()
{
    // Enable GPIO Ports A, C, D
    GPIO_port_enable(GPIO_port_A);
    GPIO_port_enable(GPIO_port_C);
    GPIO_port_enable(GPIO_port_D);
    // Set Pin Modes
    GPIO_pinMode(SW1_PIN, GPIO_pinMode_I_pullUp, GPIO_Speed_10MHz);
    GPIO_pinMode(SW2_PIN, GPIO_pinMode_I_pullUp, GPIO_Speed_10MHz);
    GPIO_pinMode(SW3_PIN, GPIO_pinMode_I_pullUp, GPIO_Speed_10MHz);
    GPIO_pinMode(ADC_PIN, GPIO_pinMode_I_analog, GPIO_Speed_10MHz);
	GPIO_pinMode(LED_PIN, GPIO_pinMode_O_pushPull, GPIO_Speed_10MHz);
	GPIO_digitalWrite(LED_PIN, low);

	GPIO_pinMode(TEST_PIN, GPIO_pinMode_O_pushPull, GPIO_Speed_10MHz);
	GPIO_digitalWrite(TEST_PIN, low);

	GPIO_pinMode(UART_PIN, GPIO_pinMode_O_pushPullMux, GPIO_Speed_10MHz);
	RCC->APB2PCENR |= RCC_APB2Periph_AFIO;

	GPIO_ADCinit();

    return 0;
}

//==================================================================
//	chack switch
//==================================================================
int check_input()
{
    int ret = 0;
	if (!GPIO_digitalRead(SW1_PIN)) {               // up sw
        ret = 1;
    } else if (!GPIO_digitalRead(SW3_PIN)) {        // mode sw
        ret = 3;
	} else if (!GPIO_digitalRead(SW2_PIN)) {        // mode sw
        ret = 2;
	} else {
        ret = 0;
    }
    return ret;
}

uint16_t adc_read_raw()
{
	return (uint16_t)GPIO_analogRead(GPIO_Ain0_A2);
}

uint16_t adc_capture_u8(int8_t *dst, uint16_t samples, uint16_t sample_period_us)
{
	uint32_t sum = 0;
	if (!dst || samples == 0) {
		return 0;
	}

	for (uint16_t i = 0; i < samples; i++) {
		uint32_t t = micros();
		uint8_t val = (uint8_t)((GPIO_analogRead(GPIO_Ain0_A2) >> 2) & 0xFF);
		sum += val;
		dst[i] = (int8_t)val;
		while ((micros() - t) < sample_period_us) {
		}
	}
	return (uint16_t)(sum / samples);
}

void gpio_write_led(uint8_t level)
{
	if (level == GPIO_HIGH) {
		GPIO_digitalWrite(LED_PIN, high);
	} else {
		GPIO_digitalWrite(LED_PIN, low);
	}
}

void gpio_write_test(uint8_t level)
{
	if (level == GPIO_HIGH) {
		GPIO_digitalWrite(TEST_PIN, high);
	} else {
		GPIO_digitalWrite(TEST_PIN, low);
	}
}

