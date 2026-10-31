#pragma once
#include <stdint.h>
#include <string.h>
extern uint32_t host_millis(void);
#define millis() host_millis()
static const uint8_t GPIO_LOW = 0;
static const uint8_t GPIO_HIGH = 1;
#define SAMPLES 128
#define BUFSIZE (SAMPLES * 2)
extern uint16_t adc_read_raw();
extern void gpio_write_led(uint8_t level);
extern uint16_t sampling_period_us;
extern int check_input();
extern void tim1_pwm_init(void);
extern void tim1_pwm_stop(void);
extern "C" void TIM1_UP_IRQHandler(void);
#define TEST_LOW
#define TEST_HIGH
