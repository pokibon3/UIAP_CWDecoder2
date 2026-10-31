//
//	CH32V cw_decoder のホストハーネス
//
//	src/cw_decoder.cpp の本物のコード (cwDecoder() のループ) をそのまま動かす。
//	ハードウェア依存の部分だけをスタブにしてある:
//	  - check_input() が呼ばれるたびに 1 ブロック (48 サンプル) を
//	    TIM1_UP_IRQHandler() 経由で流し込む (実機の 8192Hz 割り込みの代わり)
//	  - millis() は流し込んだサンプル数から作る (ブロックの終わりの時刻)
//	  - 表示へ送られた文字を集めて最後に標準出力へ書く
//
//	使い方: cwhost003 <トーン 0..4> <ピーク 0..1>  < s16le モノラル 8192Hz
//	  トーン: 0=600 1=700 2=800 3=900 4=1000Hz (実機の SW2 と同じ)
//	  ピーク: 入力の最大振幅を ADC の中点から端までの何割にするか
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>
#include "common.h"
#include "ch32fun.h"
#include "cw_decoder.h"
#include "cw_display.h"
#include "goertzel.h"

struct host_tim host_tim1;
uint8_t shared_buf[BUFSIZE];
uint16_t sampling_period_us;
static std::vector<int16_t> in;
static size_t pos = 0;
static uint64_t fed = 0;
static int adc_mid, adc_max;
static double gain;
static std::string out;

uint32_t host_millis(void) { return (uint32_t)(fed * 1000 / 8192); }

uint16_t adc_read_raw()
{
	int v = adc_mid + (int)(in[pos++] * gain);
	if (v < 0) v = 0;
	if (v > adc_max) v = adc_max;
	return (uint16_t)v;
}

int check_input()
{
	if (pos + 48 > in.size()) return 1;   // 入力が尽きたら SW1 を押したことにして抜ける
	for (int i = 0; i < 48; i++) {
		TIM1_UP_IRQHandler();
		fed++;
	}
	return 0;
}

void gpio_write_led(uint8_t) {}
void tim1_pwm_init(void) {}
void tim1_pwm_stop(void) {}
void cw_display_setup(void) {}
void cw_display_reset_decoder_view(void) {}
void cw_display_update_info(uint16_t, uint8_t, int16_t) {}
void cw_display_enqueue_char(int16_t c) { out.push_back((char)c); }
void cw_display_tick(void) {}
void cw_display_draw_magnitude(int32_t) {}
extern void host_set_tone(int);

int main(int argc, char **argv)
{
	int tone = argc > 1 ? atoi(argv[1]) : 0;
	double peak = argc > 2 ? atof(argv[2]) : 0.5;
#if defined(BOARD_CH32V006)
	adc_mid = 2048; adc_max = 4095;     // 12 ビット ADC
#else
	adc_mid = 512; adc_max = 1023;      // 10 ビット ADC
#endif
	int16_t buf[4096];
	size_t n;
	while ((n = fread(buf, 2, 4096, stdin)) > 0) in.insert(in.end(), buf, buf + n);
	int m = 1;
	for (int16_t v : in) {
		int a = v < 0 ? -v : v;
		if (a > m) m = a;
	}
	gain = peak * adc_mid / m;
	cwd_setup();
	host_set_tone(tone);    // cwd_setup() は speed=0 のままなので後から合わせる
	cwDecoder();
	printf("%s\n", out.c_str());
	return 0;
}
