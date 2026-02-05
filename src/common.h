//
//	CW Decoder Common header
//
#pragma once
#include <stdint.h>
#define GPIO_ADC_MUX_DELAY 100
#define GPIO_ADC_sampletime GPIO_ADC_sampletime_43cy

#define SCALE 6

#define micros() (SysTick->CNT / DELAY_US_TIME)
#define millis() (SysTick->CNT / DELAY_MS_TIME)

// TFT selection (set TFT_ST7735 or TFT_ST7789 via build flags)

#if (defined(TFT_ST7735) && defined(TFT_ST7789))
#error "Define only one of TFT_ST7735 or TFT_ST7789."
#endif

#ifndef TFT_CONFIG_ONLY
#if defined(TFT_ST7735)
#include "st7735.h"
#define tft_init st7735_init
#define tft_set_cursor st7735_set_cursor
#define tft_set_color st7735_set_color
#define tft_set_background_color st7735_set_background_color
#define tft_print_char st7735_print_char
#define tft_print st7735_print
#define tft_print_number st7735_print_number
#define tft_draw_pixel st7735_draw_pixel
#define tft_draw_line st7735_draw_line
#define tft_draw_rect st7735_draw_rect
#define tft_fill_rect st7735_fill_rect
#define tft_draw_bitmap st7735_draw_bitmap
#define TFT_WIDTH  ST7735_WIDTH
#define TFT_HEIGHT ST7735_HEIGHT
#define TFT_FONT_W 5
#define TFT_FONT_H 7
#define TFT_FONT_ADV 6
#elif defined(TFT_ST7789)
#include "st7789.h"
#define tft_init st7789_init
#define tft_set_cursor st7789_set_cursor
#define tft_set_color st7789_set_color
#define tft_set_background_color st7789_set_background_color
#define tft_print_char st7789_print_char
#define tft_print st7789_print
#define tft_print_number st7789_print_number
#define tft_draw_pixel st7789_draw_pixel
#define tft_draw_line st7789_draw_line
#define tft_draw_rect st7789_draw_rect
#define tft_fill_rect st7789_fill_rect
#define tft_draw_bitmap st7789_draw_bitmap
#define TFT_WIDTH  ST7789_WIDTH
#define TFT_HEIGHT ST7789_HEIGHT
#define TFT_FONT_W 5
#define TFT_FONT_H 7
#define TFT_FONT_ADV 6
#else
#error "Define TFT_ST7735 or TFT_ST7789."
#endif
#endif

#define SW1_PIN GPIOv_from_PORT_PIN(GPIO_port_A, 1)		// for uiap
#define SW2_PIN GPIOv_from_PORT_PIN(GPIO_port_C, 4)		// for uiap
#define SW3_PIN GPIOv_from_PORT_PIN(GPIO_port_D, 2)
#define ADC_PIN GPIOv_from_PORT_PIN(GPIO_port_A, 2)		// for uiap
#define LED_PIN GPIOv_from_PORT_PIN(GPIO_port_C, 0)		// for uiap
#define UART_PIN GPIOv_from_PORT_PIN(GPIO_port_D, 5)
#define TEST_PIN GPIOv_from_PORT_PIN(GPIO_port_D, 6)

#define TEST_HIGH			GPIO_digitalWrite(TEST_PIN, high);
#define TEST_LOW			GPIO_digitalWrite(TEST_PIN, low);

#define SAMPLES 128
#define BUFSIZE (SAMPLES * 2)

extern uint16_t sampling_period_us;

extern "C" int mini_snprintf(char* buffer, unsigned int buffer_len, const char *fmt, ...);
extern int GPIO_setup();
extern int check_input();

extern void tim1_pwm_init(void);
extern void tim1_pwm_stop(void);

extern "C" void TIM1_UP_IRQHandler(void) __attribute__((interrupt));
