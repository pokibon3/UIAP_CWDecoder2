///////////////////////////////////////////////////////////////////////
// CWデコーダ (Hjalmar Skovholm Hansen OZ1JHM) バージョン 1.01       //
// 自由に改変・複製できますが、GPL を遵守してください。       //
// ライセンス: <a href="http://www.gnu.org/copyleft/gpl.html" title="http://www.gnu.org/copyleft/gpl.html" rel="nofollow">http://www.gnu.org/copyleft/gpl.html</a>              //
// 議論・提案はこちら:                                               //
// <a href="https://groups.yahoo.com/neo/groups/oz1jhm/conversations/messages" title="https://groups.yahoo.com/neo/groups/oz1jhm/conversations/messages" rel="nofollow">https://groups.yahoo.com/neo/groups/oz1jhm/conversations/messages</a> //
//                                                                   //
// KC2UEZ による改変 (バージョン 1.2):                                     //
// - Arduino NANO に対応                                            //
// - 起動時に「ターゲット周波数」と「帯域」を選択可能               //
///////////////////////////////////////////////////////////////////////
 
///////////////////////////////////////////////////////////////////////////
// Goertzel 法の解説: <a href="http://en.wikipedia.org/wiki/Goertzel_algorithm" title="http://en.wikipedia.org/wiki/Goertzel_algorithm" rel="nofollow">http://en.wikipedia.org/wiki/Goertzel_algorithm</a>     //
// FFT の参考: <a href="http://www.dspguide.com/pdfbook.htm" title="http://www.dspguide.com/pdfbook.htm" rel="nofollow">http://www.dspguide.com/pdfbook.htm</a>                //
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// 改変版 (Kimio Ohe)
// 変更点:
//  - UIAPduino Pro Micro(CH32V003) に移植
//  - 周波数検出機能を追加
//  - 全ソースをリファクタリング
// 日付: 2025-11-07 バージョン 1.0
//  - DFT を Goertzel 法に変更
// 日付: 2025.12.05 バージョン 1.1
//  - オーディオ入力ダイナミックレンジ拡大
// 日付: 2025.12.13 バージョン 1.2
//  - DFTの周波数を調整
//  - DFTの入力レベルを調整
// 日付: 2025.12.20 バージョン 1.3
//  - デコードタイミング微調整
//
// このソフトウェアは GNU General Public License (GPL) に基づき配布されています。
// 改変版も同じ GPL ライセンスで再配布してください。
///////////////////////////////////////////////////////////////////////////
//
//	CWデコーダ関数
//
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "goertzel.h"
#include "decode.h"
#include "ch32v003fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "cw_display.h"

//#define SERIAL_OUT

#define TEST_HIGH			GPIO_digitalWrite(TEST_PIN, high);
#define TEST_LOW			GPIO_digitalWrite(TEST_PIN, low);

#define GOERTZEL_SAMPLES 48
#define GOERTZEL_SAMPLING_FREQUENCY 8192
#define NOISE_BLANKER_ENABLED 1
static uint16_t magnitudelimit = 140;  		// 以前は 140
static uint16_t magnitudelimit_low = 140;
static uint16_t realstate = low;
static uint16_t realstatebefore = low;
static uint16_t filteredstate = low;
static uint16_t filteredstatebefore = low;
static uint32_t starttimehigh;
static uint32_t highduration;
static uint32_t lasthighduration;
static uint32_t hightimesavg = 60;
static uint32_t startttimelow;
static uint32_t lowduration;
static uint32_t laststarttime = 0;
static uint16_t nbtime = 6;  /// ノイズブランカの時間(ms)

static char code[20];
static uint16_t stop = low;
static uint16_t wpm;

static char		sw = MODE_US;
static int16_t 	speed = 0;

uint8_t lastChar = 0;

extern uint8_t shared_buf[BUFSIZE];
static volatile uint32_t morseSum[2];
static volatile uint16_t morseWriteIndex = 0;
static volatile uint8_t morseWriteBuf = 0;
static volatile uint8_t morseReady[2] = { 0, 0 };

static inline int16_t *get_morse_buf(uint8_t idx)
{
	return (int16_t *)shared_buf + (idx * GOERTZEL_SAMPLES);
}

static void reset_morse_buffers(void)
{
	morseSum[0] = 0;
	morseSum[1] = 0;
	morseWriteIndex = 0;
	morseWriteBuf = 0;
	morseReady[0] = 0;
	morseReady[1] = 0;
}

extern "C" void TIM1_UP_IRQHandler(void) __attribute__((interrupt));
extern "C" void TIM1_UP_IRQHandler(void)
{
	if (TIM1->INTFR & TIM_IT_Update) {
		TIM1->INTFR = (uint16_t)~TIM_IT_Update;
	}

	if (morseReady[morseWriteBuf]) {
		uint8_t next = morseWriteBuf ^ 1;
		if (!morseReady[next]) {
			morseWriteBuf = next;
			morseWriteIndex = 0;
			morseSum[morseWriteBuf] = 0;
		} else {
			return;
		}
	}

	if (morseWriteIndex >= GOERTZEL_SAMPLES) {
		morseWriteIndex = 0;
		morseSum[morseWriteBuf] = 0;
	}

	{
		uint16_t sample = (uint16_t)(GPIO_analogRead(GPIO_Ain0_A2) >> 1);
		get_morse_buf(morseWriteBuf)[morseWriteIndex] = (int16_t)sample;
		morseSum[morseWriteBuf] += sample;
		morseWriteIndex++;
		if (morseWriteIndex >= GOERTZEL_SAMPLES) {
			morseReady[morseWriteBuf] = 1;
			uint8_t next = morseWriteBuf ^ 1;
			if (!morseReady[next]) {
				morseWriteBuf = next;
				morseWriteIndex = 0;
				morseSum[morseWriteBuf] = 0;
			} else {
				morseWriteIndex = GOERTZEL_SAMPLES;
			}
		}
	}
}

//==================================================================
// gap を 1単位 hightimesavg の相対値で分類
//==================================================================
typedef enum {
	GAP_INTRA = 0, // 文字内
	GAP_CHAR  = 1, // 文字間
	GAP_WORD  = 2  // 単語間
} gap_type_t;

static gap_type_t classify_gap(uint32_t gap, uint32_t unit)
{
	if (unit == 0) return GAP_INTRA; // まだ学習前の保護

	// gap < 1.5 * unit → 同一文字内
	if (gap < (unit * 3) / 2) {
		return GAP_INTRA;
	}
	// 1.5〜4.5 * unit → 文字間
	if (gap < (unit * 9) / 2) {
		return GAP_CHAR;
	}
	// 6 * unit 以上 → 単語間
	if (gap >= unit * 6) {
		return GAP_WORD;
	}

	// 4.5〜6 * unit は微妙ゾーン → とりあえず文字間に寄せる
	return GAP_CHAR;
}

//==================================================================
//	ノイズブランカ時間を短点長から算出
//==================================================================
static uint16_t compute_nbtime(uint32_t unit_ms)
{
	if (unit_ms == 0) return 10;
	uint32_t t = unit_ms / 5; // 1/5 を基本
	if (t < (unit_ms / 3)) {
		// 1/5〜1/3の範囲に寄せる
		uint32_t max_t = unit_ms / 3;
		if (t > max_t) t = max_t;
	}
	if (t < 3) t = 3;
	// Cap debounce to allow up to 50 WPM (dot ~= 24ms, debounce <= ~1/3).
	if (t > 8) t = 8;
	return (uint16_t)t;
}
//==================================================================

//==================================================================
//	スイッチ入力の確認
//==================================================================
static int check_sw()
{
    int ret = 0;
	int val = check_input();

	if (val == 3) {
		speed += 1;
		if (speed > 2) speed = 0;
		setSpeed(speed);
		cw_display_update_info(wpm, (uint8_t)sw, speed);
		Delay_Ms(300);
	} else if (val == 2) {
		sw ^= 1;
		cw_display_update_info(wpm, (uint8_t)sw, speed);
		Delay_Ms(300);
    } else if (val == 1) {
		Delay_Ms(300);
        ret = 1;
	}
    return ret;
}

//==================================================================
//	cw_decoder 初期化
//==================================================================
int cwd_setup()
{
	cw_display_setup();
	initGoertzel(speed);
	sampling_period_us = 900000 / GOERTZEL_SAMPLING_FREQUENCY;
	reset_morse_buffers();
	tim1_pwm_init();
	return 0;
}


//==================================================================
//	cwDecoder : デコーダ本体
//==================================================================
static int decodeAscii(int16_t asciinumber)
{
	if (asciinumber == 0) return 0;
	if (lastChar == 32 && asciinumber == 32) return 0;

	if        (asciinumber == 1) {			// AR
		cw_display_print_ascii('A');
		cw_display_print_ascii('R');
	} else if (asciinumber == 2) {			// KN
		cw_display_print_ascii('K');
		cw_display_print_ascii('N');
	} else if (asciinumber == 3) {			// BT
		cw_display_print_ascii('B');
		cw_display_print_ascii('T');
	} else if (asciinumber == 4) {			// VA
		cw_display_print_ascii('V');
		cw_display_print_ascii('A');
	} else if (asciinumber == 7) {			// HH (訂正)
		cw_display_print_ascii('H');
		cw_display_print_ascii('H');
	} else {
		cw_display_print_ascii(asciinumber);
	}
	lastChar = asciinumber;

	return 0;
}

//==================================================================
//	cwDecoder : デコーダ本体
//==================================================================
int cwDecoder(void)
{
	int32_t magnitude;
	cw_display_reset_decoder_view();

	while(1) {
		if (check_sw() == 1) {
			break;
		}
		if (!morseReady[0] && !morseReady[1]) {
			continue;
		}

		int buf_idx = morseReady[0] ? 0 : 1;
		int16_t *morseData = get_morse_buf((uint8_t)buf_idx);
		int16_t ave = (int16_t)(morseSum[buf_idx] / GOERTZEL_SAMPLES);
		for (int i = 0; i < GOERTZEL_SAMPLES; i++) {
			morseData[i] -= ave;
		}

		// Goertzel 計算
		magnitude = goertzel(morseData, GOERTZEL_SAMPLES);
//TEST_LOW
		cw_display_draw_magnitude(magnitude);
#ifdef SERIAL_OUT
		printf("mag = %d\n", magnitude);
#endif
		///////////////////////////////////////////////////////////
		// 振幅しきい値を自動更新
		///////////////////////////////////////////////////////////
  		if (magnitude > magnitudelimit_low){
			magnitudelimit = (magnitudelimit +((magnitude - magnitudelimit)/6));  /// 移動平均フィルタ
  		}
  		if (magnitudelimit < magnitudelimit_low) {
			magnitudelimit = magnitudelimit_low;
		}

		////////////////////////////////////
		// 振幅でしきい値判定
		////////////////////////////////////
		if(magnitude > magnitudelimit*0.6) {  // 余裕を持たせる
     		realstate = high;
		} else {
    		realstate = low;
		}

		/////////////////////////////////////////////////////
		// ノイズブランカで状態を安定化
		/////////////////////////////////////////////////////
		if (realstate != realstatebefore){
			laststarttime = millis();
		}
#if NOISE_BLANKER_ENABLED
		{
			uint32_t unit = (hightimesavg > 0) ? hightimesavg : highduration;
			nbtime = compute_nbtime(unit);
		}
		// Bypass debounce for the first rising edge after a long gap.
		{
			uint32_t unit = (hightimesavg > 0) ? hightimesavg : highduration;
			if (filteredstate == low && realstate == high && lowduration > unit * 6) {
				filteredstate = realstate;
			}
		}
		if ((millis()-laststarttime)> nbtime) {
			if (realstate != filteredstate) {
				filteredstate = realstate;
			}
		}
#else
		filteredstate = realstate;
#endif

		////////////////////////////////////////////////////////////
		// HIGH/LOW の継続時間を計測
		////////////////////////////////////////////////////////////
		if (filteredstate != filteredstatebefore) {
			if (filteredstate == high) {
				starttimehigh = millis();
				lowduration = (millis() - startttimelow);
			}
			if (filteredstate == low) {
				startttimelow = millis();
				highduration = (millis() - starttimehigh);
				if (highduration < (2*hightimesavg) || hightimesavg == 0) {
					hightimesavg = (highduration+hightimesavg+hightimesavg) / 3;     // dot avg (3-sample moving avg)
				}
				if (highduration > (5*hightimesavg) ) {
					hightimesavg = highduration+hightimesavg;     // follow sudden speed drop
				}
				if (hightimesavg < 24) {
					hightimesavg = 24;
				}
			}
		}

		///////////////////////////////////////////////////////////////
		// 短点/長点判定と休止(1/3/7単位)の判定
		// 1/3/7 単位の休止を判定
		// hightimesavg を 1単位(短点)とみなす
		///////////////////////////////////////////////////////////////
		if (filteredstate != filteredstatebefore){
			stop = low;
			if (filteredstate == low){  //// HIGH 終了
				if (highduration < (hightimesavg*2) && highduration > (hightimesavg*0.6)){ /// 0.6 未満はノイズ除外
					strcat(code,".");
//					printf(".");
				}
				if (highduration > (hightimesavg*2) && highduration < (hightimesavg*6)){
					strcat(code,"-");
//					printf("-");
				wpm = (wpm + (1200/((highduration)/3)))/2;  //// 可能な限り精度の高い推定
					if (wpm > 50) {
						wpm = 50;
					}
				}
			}
		}
		if (filteredstate == high) {  //// LOW 終了

			if (hightimesavg > 0) {
				gap_type_t g = classify_gap(lowduration, hightimesavg);

				if (g == GAP_CHAR) {          // 文字間
					if (strlen(code) > 0) {
						decodeAscii(decode(code, &sw));
						code[0] = '\0';
					}
				} else if (g == GAP_WORD) {   // 単語間
					if (strlen(code) > 0) {
						decodeAscii(decode(code, &sw));
						code[0] = '\0';
					}
					decodeAscii(32);           // スペース出力
				}
			}
		}

		//////////////////////////////
		// 一定時間無音なら確定出力
		//////////////////////////////
		uint32_t unit = (hightimesavg > 0) ? hightimesavg : highduration;
		if ((millis() - startttimelow) > unit * 6 && stop == low) {
			decodeAscii(decode(code, &sw));
			code[0] = '\0';
			stop = high;
		}

		/////////////////////////////////////
		// LED の点灯/消灯
		// スピーカ制御(未使用)
		/////////////////////////////////////
		if(filteredstate == high){
			GPIO_digitalWrite(LED_PIN, high);
		} else {
			GPIO_digitalWrite(LED_PIN, low);
		}

		//////////////////////////////////
		// ループ終端の状態更新
		/////////////////////////////////
		cw_display_update_info(wpm, (uint8_t)sw, speed);
		realstatebefore = realstate;
		lasthighduration = highduration;
		filteredstatebefore = filteredstate;
		morseReady[buf_idx] = 0;
	}
	TIM1->DMAINTENR &= ~TIM_IT_Update;
	TIM1->CTLR1 &= ~TIM_CEN;
    return 0;
}
