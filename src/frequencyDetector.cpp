//
//	Frequency Detector functions
//
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "frequencyDetector.h"
#include "fix_fft.h"
#include "ch32fun.h"

#define FD_SAMPLING_FREQUENCY 6000	// Hz	+10%
#define FFT_FPS_MEASURE 0

// integer sqrt: floor(sqrt(x))
static uint32_t isqrt32(uint32_t x)
{
	uint32_t op = x;
	uint32_t res = 0;
	uint32_t one = 1u << 30;

	while (one > op) {
		one >>= 2;
	}
	while (one != 0) {
		if (op >= res + one) {
			op -= res + one;
			res += one << 1;
		}
		res >>= 1;
		one >>= 2;
	}
	return res;
}

// 画面レイアウト用マクロ
#define FFT_FRAME_HEIGHT    (TFT_HEIGHT - 8)
#define FFT_AREA_Y_TOP      ((TFT_HEIGHT * 19) / 80)
#define FFT_AREA_Y_BOTTOM   ((TFT_HEIGHT * 70) / 80)
#define FFT_AREA_HEIGHT     (FFT_AREA_Y_BOTTOM - FFT_AREA_Y_TOP + 1)
#define FFT_LABEL_Y         (FFT_FRAME_HEIGHT + 1)

// タイトル文字列
//static char title1[]   = "Freq.Detector";
//static char title2[]   = "  for UIAP   ";
//static char title3[]   = " Version 1.0";


//==================================================================
//	chack switch
//==================================================================
static int check_sw()
{
    int ret = 0;
	int val = check_input();

	if (val == 1) {
		Delay_Ms(300);
        ret = 1;
	}

    return ret;
}


//==================================================================
//	freq_detector setup
//==================================================================
int fd_setup()
{
    tim1_pwm_stop();
    sampling_period_us = 900000L / FD_SAMPLING_FREQUENCY;
    //sampling_period_us = 1000000L / FD_SAMPLING_FREQUENCY;

	// display title
	tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);

/*
	tft_set_color(BLUE);
	tft_set_cursor(0, 10);
	tft_print(title1, FONT_SCALE_16X16);

	tft_set_color(RED);
	tft_set_cursor(0, 30);
	tft_print(title2, FONT_SCALE_16X16);

	tft_set_cursor(0, 50);
	tft_set_color(GREEN);
	tft_print(title3, FONT_SCALE_16X16);
	Delay_Ms( 1000 );
*/
	tft_set_color(WHITE);


    return 0;
}

//==================================================================
//  adc and fft for freq counter
//==================================================================
int freqDetector(int8_t *vReal, int8_t *vImag) 
{
	uint16_t peakFrequency = 0;
	uint16_t oldFreequency = 0;
	char buf[16];

	tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
	tft_draw_rect(0, 0, TFT_WIDTH, FFT_FRAME_HEIGHT, BLUE);

	uint8_t  fft_bins = (uint8_t)(SAMPLES / 2);
#if !defined(TFT_ST7735)
	if (fft_bins > 64) fft_bins = 64;
#endif
	uint16_t bin_step = (TFT_WIDTH - 1) / fft_bins;
	if (bin_step < 3) bin_step = 3;
	uint16_t bar_width = 1;
#if defined(TFT_ST7735)
	bar_width = 4;
	bin_step = (bar_width > 1) ? (bar_width - 1) : 1;
	{
		const uint8_t max_bins = (uint8_t)((TFT_WIDTH - 2) / bin_step);
		if (max_bins < fft_bins) fft_bins = max_bins;
	}
#else
	{
		const uint16_t usable_width = (TFT_WIDTH > 2) ? (uint16_t)(TFT_WIDTH - 2) : 0;
		const uint16_t target_bins = 59;
		if (fft_bins > target_bins) fft_bins = (uint8_t)target_bins;
		uint16_t w = (fft_bins > 0) ? (uint16_t)(usable_width / fft_bins) : 1;
		if (w < 1) w = 1;
		bar_width = (uint16_t)(w + 1);
		bin_step = (bar_width > 1) ? (bar_width - 1) : 1;
		{
			const uint8_t max_bins = (uint8_t)(usable_width / bin_step);
			if (max_bins < fft_bins) fft_bins = max_bins;
		}
	}
#endif
	uint16_t plot_width = bin_step * fft_bins;
	if (plot_width > (TFT_WIDTH - 2)) {
		plot_width = TFT_WIDTH - 2;
	}
	uint16_t plot_left = (TFT_WIDTH - plot_width) / 2;
	const uint16_t bin_hz = (uint16_t)(FD_SAMPLING_FREQUENCY / SAMPLES);
	uint16_t bin1 = (uint16_t)((1000 + (bin_hz / 2)) / bin_hz);
	uint16_t bin2 = (uint16_t)((2000 + (bin_hz / 2)) / bin_hz);
	if (bin1 >= fft_bins) bin1 = (fft_bins > 0) ? (uint16_t)(fft_bins - 1) : 0;
	if (bin2 >= fft_bins) bin2 = (fft_bins > 0) ? (uint16_t)(fft_bins - 1) : 0;
	if (bin1 == 0) bin1 = 1;
	if (bin2 == 0) bin2 = 1;
	const uint16_t bar_center_offset = (uint16_t)((bar_width > 0) ? ((bar_width - 1) / 2) : 0);
	uint16_t line1_x = plot_left + (bin1 * bin_step) + bar_center_offset;
	uint16_t line2_x = plot_left + (bin2 * bin_step) + bar_center_offset;
	uint16_t value_area_x = plot_left + (plot_width / 2);
	uint16_t value_text_base = value_area_x + 2;

	// Place labels near guide lines.
	tft_set_color(BLUE);
	const char* label0 = "0Hz";
	const char* label1 = "1KHz";
	const char* label2 = "2KHz";
	const uint8_t label_char_w = TFT_FONT_ADV;
	uint16_t label_w0 = (uint16_t)(strlen(label0) * label_char_w);
	uint16_t label_w1 = (uint16_t)(strlen(label1) * label_char_w);
	uint16_t label_w2 = (uint16_t)(strlen(label2) * label_char_w);
	uint16_t x0 = (plot_left > (label_w0 / 2)) ? (plot_left - (label_w0 / 2)) : 0;
	uint16_t x1 = (line1_x > (label_w1 / 2)) ? (line1_x - (label_w1 / 2)) : 0;
	uint16_t x2 = (line2_x > (label_w2 / 2)) ? (line2_x - (label_w2 / 2)) : 0;
	if (x0 + label_w0 > TFT_WIDTH) x0 = (uint16_t)(TFT_WIDTH - label_w0);
	if (x1 + label_w1 > TFT_WIDTH) x1 = (uint16_t)(TFT_WIDTH - label_w1);
	if (x2 + label_w2 > TFT_WIDTH) x2 = (uint16_t)(TFT_WIDTH - label_w2);
	tft_set_cursor(x0, FFT_LABEL_Y);
	tft_print(label0, FONT_SCALE_8X8);
	tft_set_cursor(x1, FFT_LABEL_Y);
	tft_print(label1, FONT_SCALE_8X8);
	tft_set_cursor(x2, FFT_LABEL_Y);
	tft_print(label2, FONT_SCALE_8X8);

	while(1) {
		uint16_t ave = 0;
#if FFT_FPS_MEASURE
		static uint32_t fps_last_ms = 0;
		static uint16_t fps_frames = 0;
		static uint16_t fps_value = 0;
#endif

		if (check_sw() == 1) {
            break;
        }
TEST_HIGH
		// input audio
		ave = adc_capture_u8(vImag, SAMPLES, sampling_period_us);
TEST_LOW
		//printf("ave = %d\n", ave);
		for (int i = 0; i < SAMPLES; i++) {
			vReal[i] = (int8_t)(vImag[i] - ave);
			vImag[i] = 0; // Imaginary partは0に初期化
		}
//TEST_HIGH
		// FFT
  		fix_fft((char *)vReal, (char *)vImag, 7, 0); // SAMPLES = 256なので、log2(SAMPLES) = 8
//TEST_HIGH
  		// Magnitude Calculation
		for (int i = 0; i < SAMPLES / 2; i++) {
			int16_t vr = vReal[i];
			int16_t vi = vImag[i];
			uint32_t mag2 = (uint32_t)(vr * vr) + (uint32_t)(vi * vi);
			vReal[i] = (int8_t)isqrt32(mag2);
		}
		// draw FFT result (DMA scanline)
		uint8_t maxIndex = 0;
		uint8_t maxValue = 0;
		static uint8_t bar_x[64];
		static uint8_t bar_h[64];
		static uint8_t linebuf[TFT_WIDTH * 2] = {0};
		const uint16_t line_width = (TFT_WIDTH > 2) ? (uint16_t)(TFT_WIDTH - 2) : 0;

		tft_fill_rect(1, FFT_AREA_Y_TOP, TFT_WIDTH - 2, FFT_AREA_HEIGHT, BLACK);

		for (int i = 1; i < fft_bins; i++) {
			int16_t val = vReal[i] * SCALE;
			if (val > (int16_t)(FFT_AREA_HEIGHT - 1)) val = (int16_t)(FFT_AREA_HEIGHT - 1);
			bar_x[i] = (uint8_t)(plot_left + (i * bin_step));
			bar_h[i] = (uint8_t)val;
			if (vReal[i] > maxValue) {
				maxValue = vReal[i];
				maxIndex = i;
			}
		}

		for (uint16_t y = FFT_AREA_Y_TOP; y <= FFT_AREA_Y_BOTTOM; y++) {
			uint16_t idx = 0;
			for (uint16_t x = 1; x < TFT_WIDTH - 1; x++) {
				linebuf[idx++] = 0;
				linebuf[idx++] = 0;
			}
			for (int i = 1; i < fft_bins; i++) {
				uint8_t h = bar_h[i];
				if (h == 0) continue;
				uint16_t top = (uint16_t)(FFT_AREA_Y_BOTTOM - h);
				uint16_t x0 = bar_x[i];
				uint16_t x1 = (uint16_t)(x0 + bar_width - 1);
				if (y == top || y == FFT_AREA_Y_BOTTOM) {
					for (uint16_t dx = 0; dx < bar_width; dx++) {
						uint16_t px = (uint16_t)(x0 + dx);
						if (px >= 1 && px < (TFT_WIDTH - 1)) {
							uint16_t off = (uint16_t)((px - 1) * 2);
							linebuf[off] = (uint8_t)(WHITE >> 8);
							linebuf[off + 1] = (uint8_t)WHITE;
						}
					}
				} else if (y > top && y < FFT_AREA_Y_BOTTOM) {
					if (x0 >= 1 && x0 < (TFT_WIDTH - 1)) {
						uint16_t off = (uint16_t)((x0 - 1) * 2);
						linebuf[off] = (uint8_t)(WHITE >> 8);
						linebuf[off + 1] = (uint8_t)WHITE;
					}
					if (x1 >= 1 && x1 < (TFT_WIDTH - 1)) {
						uint16_t off = (uint16_t)((x1 - 1) * 2);
						linebuf[off] = (uint8_t)(WHITE >> 8);
						linebuf[off + 1] = (uint8_t)WHITE;
					}
				}
			}
			if (line_width > 0) {
				tft_draw_bitmap(1, y, line_width, 1, linebuf);
			}
		}

		tft_draw_line(line1_x, 1, line1_x, FFT_FRAME_HEIGHT - 1, DARKBLUE); // 1.0kHz line
		tft_draw_line(line2_x, 1, line2_x, FFT_FRAME_HEIGHT - 1, DARKBLUE); // 2.0kHz line
		// disp freqeuncy
		peakFrequency = (FD_SAMPLING_FREQUENCY / SAMPLES) * maxIndex;
		if (peakFrequency != oldFreequency) {
			if (maxValue >= 4 ) {
				mini_snprintf(buf, sizeof(buf), "%4dHz", peakFrequency + FD_SAMPLING_FREQUENCY / SAMPLES / 2);
			} else {
				strcpy(buf, "    Hz");
			}
			tft_set_cursor((6 - strlen(buf)) * 12 + value_text_base, 3);
			tft_set_color(YELLOW);
			tft_fill_rect(value_area_x, 1, (plot_left + plot_width) - value_area_x - 2, 18, BLACK);
			tft_print(buf, FONT_SCALE_16X16);
			
			oldFreequency = peakFrequency;
		}
#if FFT_FPS_MEASURE
		fps_frames++;
		{
			uint32_t now = millis();
			if (fps_last_ms == 0) fps_last_ms = now;
			if ((now - fps_last_ms) >= 1000) {
				fps_value = fps_frames;
				fps_frames = 0;
				fps_last_ms = now;
				tft_set_color(GREEN);
				tft_fill_rect(2, 2, 6 * 8, 8, BLACK);
				tft_set_cursor(2, 2);
				tft_print("FPS:", FONT_SCALE_8X8);
				{
					char fps_buf[8];
					mini_snprintf(fps_buf, sizeof(fps_buf), "%u", fps_value);
					tft_print(fps_buf, FONT_SCALE_8X8);
				}
				tft_set_color(WHITE);
			}
		}
#endif
//TEST_LOW
	}
	return 0;
}
