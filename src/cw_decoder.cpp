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

//#define SERIAL_OUT

#define TEST_HIGH			GPIO_digitalWrite(TEST_PIN, high);
#define TEST_LOW			GPIO_digitalWrite(TEST_PIN, low);

#define GOERTZEL_SAMPLES 48
#define GOERTZEL_SAMPLING_FREQUENCY 8192
#if (TFT_WIDTH >= 240)
#define TEXT_SCALE 3
#define FONT_WIDTH 24
#define TEXT_ADVANCE 20
#define LINE_HEIGHT 28
const int colums = 12; /// 表示列数(本来は16/20向け)
#else
#define TEXT_SCALE 2
#define FONT_WIDTH 12
#define TEXT_ADVANCE 12
#define LINE_HEIGHT 20
const int colums = 13; /// 表示列数(本来は16/20向け)
#endif
static char title1[]   = " CW Decoder  ";
static char title2[]   = "  for UIAP   ";
static char title3[]   = " Version 1.4 ";
static uint8_t first_flg = 1;

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

//static const char *tone[] = {
//	" 700",
//	" 800",
//	"1000"
//};
static const uint16_t tone_hz[] = { 700, 800, 1000 };
static char info_last_buf[24];
static uint8_t info_sep_drawn = 0;
static uint16_t info_last_wpm = 0xFFFF;
static uint8_t info_last_sw = 0xFF;
static int16_t info_last_speed = -1;

int lcdindex = 0;
#if defined(TFT_ST7739)
static uint8_t current_line = 0;
static uint8_t line0[colums];
static uint8_t line1[colums];
static uint8_t line2[colums];
static uint8_t line3[colums];
static uint8_t* const linebufs[] = { line0, line1, line2, line3 };
#else
static uint8_t current_line = 0;
static uint8_t line0[colums];
static uint8_t line1[colums];
static uint8_t line2[colums];
static uint8_t* const linebufs[] = { line0, line1, line2 };
#endif
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
	if (t > 20) t = 20;
	return (uint16_t)t;
}
//==================================================================
//	updateinfolinelcd() : 情報行をLCDに表示
//==================================================================
static void updateinfolinelcd()
{
	char buf[24];
	const char *mode;
	uint16_t w;
	uint8_t buf_len;
	uint8_t last_len;
	uint8_t max_len;
	uint8_t force_full = 0;
	const uint16_t info_advance = (uint16_t)((8 - 2) * FONT_SCALE_16X16);
	uint16_t x;
	int16_t dirty_min = -1;
	int16_t dirty_max = -1;

	if (wpm < 10) {
		w = 0;
	} else {
		w = wpm;
	}
	if (sw == 0) {
		mode = "US";
	} else {
		mode = "JP";
	}
	if (info_sep_drawn &&
		w == info_last_wpm &&
		sw == info_last_sw &&
		speed == info_last_speed) {
		return;
	}
	if (info_sep_drawn && speed != info_last_speed) {
		force_full = 1;
	}
#if defined(TFT_ST7739)
#if (TFT_WIDTH < 240)
	mini_snprintf(buf, sizeof(buf), "%2dW %s %dHz", w, mode, tone_hz[speed]);
#else
	mini_snprintf(buf, sizeof(buf), "%2dWPM %s Mode %4dHz", w, mode, tone_hz[speed]);
#endif
#else
	mini_snprintf(buf, sizeof(buf), "%2dWPM %s%s", w, mode, tone[speed]);
#endif
	buf_len = (uint8_t)strlen(buf);
	last_len = (uint8_t)strlen(info_last_buf);
	max_len = (buf_len > last_len) ? buf_len : last_len;
	if (max_len == 0) return;
#if defined(TFT_ST7739)
	if (!info_sep_drawn) {
		uint16_t info_h = (uint16_t)(8 * FONT_SCALE_16X16);
		uint16_t sep_y = (uint16_t)(info_h + 1);
		tft_draw_line(0, sep_y, TFT_WIDTH - 1, sep_y, GREEN);
		info_sep_drawn = 1;
	}
#else
	if (!info_sep_drawn) {
		tft_draw_line(0, 17, TFT_WIDTH - 1, 17, GREEN);
		info_sep_drawn = 1;
	}
#endif
	tft_set_color(BLUE);
	if (force_full) {
		dirty_min = 0;
		dirty_max = (int16_t)max_len - 1;
	} else {
		for (uint8_t i = 0; i < max_len; i++) {
			char now = (i < buf_len) ? buf[i] : ' ';
			char old = (i < last_len) ? info_last_buf[i] : ' ';
			if (now != old) {
				if (dirty_min < 0) dirty_min = (int16_t)i;
				dirty_max = (int16_t)i;
			}
		}
		if (dirty_min < 0) {
			tft_set_color(WHITE);
			return;
		}
		if (dirty_min > 0) dirty_min--;
		if (dirty_max + 1 < (int16_t)max_len) dirty_max++;
	}
	for (int16_t i = dirty_min; i <= dirty_max; i++) {
		char now = (i < buf_len) ? buf[i] : ' ';
		x = (uint16_t)(i * info_advance);
		tft_set_cursor(x, 0);
		tft_print_char(now, FONT_SCALE_16X16);
	}
	tft_set_color(WHITE);
	memcpy(info_last_buf, buf, buf_len + 1);
	info_last_wpm = w;
	info_last_sw = (uint8_t)sw;
	info_last_speed = speed;
}

//==================================================================
//	printascii : ASCII文字をLCDに表示
//==================================================================
static void printAscii(int16_t asciinumber)
{
#ifdef SERIAL_OUT
	printf("%c", asciinumber);
#endif
#if defined(TFT_ST7739)
	uint16_t line_y[4];
	{
		const uint16_t sep_y = (uint16_t)(8 * FONT_SCALE_16X16 + 1);
		const uint16_t top = (uint16_t)(sep_y + 2);
		for (uint8_t i = 0; i < 4; i++) {
			line_y[i] = (uint16_t)(top + LINE_HEIGHT * i);
		}
	}
	if (lcdindex > colums - 1){
		lcdindex = 0;
		current_line++;
		if (current_line >= 4) {
			current_line = 3;
			for (int i = 0; i <= colums - 1 ; i++){
				line0[i] = line1[i];
				line1[i] = line2[i];
				line2[i] = line3[i];
				line3[i] = 32;
			}
			for (int i = 0; i <= colums - 1 ; i++){
				tft_set_cursor(i * TEXT_ADVANCE, line_y[0]);
				tft_print_char(line0[i], TEXT_SCALE);
				tft_set_cursor(i * TEXT_ADVANCE, line_y[1]);
				tft_print_char(line1[i], TEXT_SCALE);
				tft_set_cursor(i * TEXT_ADVANCE, line_y[2]);
				tft_print_char(line2[i], TEXT_SCALE);
				tft_set_cursor(i * TEXT_ADVANCE, line_y[3]);
				tft_print_char(32, TEXT_SCALE);
			}
		}
	}
	{
		uint8_t* line = linebufs[current_line];
		line[lcdindex] = asciinumber;
		tft_set_cursor(lcdindex * TEXT_ADVANCE, line_y[current_line]);
		tft_print_char(asciinumber, TEXT_SCALE);
	}
	lcdindex += 1;
#else
	const uint16_t base_y = (uint16_t)(8 * FONT_SCALE_16X16 + 3);
	const uint16_t line_y[] = { base_y, (uint16_t)(base_y + LINE_HEIGHT), (uint16_t)(base_y + LINE_HEIGHT * 2) };
	if (lcdindex > colums - 1){
		lcdindex = 0;
		current_line++;
		if (current_line >= 3) {
			current_line = 2;
			for (int i = 0; i <= colums - 1 ; i++){
				line0[i] = line1[i];
				line1[i] = line2[i];
				line2[i] = 32;
			}
			for (int i = 0; i <= colums - 1 ; i++){
				tft_set_cursor(i * TEXT_ADVANCE, line_y[0]);
				tft_print_char(line0[i], TEXT_SCALE);
				tft_set_cursor(i * TEXT_ADVANCE, line_y[1]);
				tft_print_char(line1[i], TEXT_SCALE);
				tft_set_cursor(i * TEXT_ADVANCE, line_y[2]);
				tft_print_char(32, TEXT_SCALE);
			}
		}
 	}
	{
		uint8_t* line = linebufs[current_line];
		line[lcdindex] = asciinumber;
		tft_set_cursor(lcdindex * TEXT_ADVANCE, line_y[current_line]);
		tft_print_char(asciinumber, TEXT_SCALE);
	}
	lcdindex += 1;
#endif
}

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
		updateinfolinelcd();
		Delay_Ms(300);
	} else if (val == 2) {
		sw ^= 1;
		updateinfolinelcd();
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
	if (first_flg == 1) {
		const uint8_t title_gap = 4;
		uint8_t max_title_len = (uint8_t)strlen(title1);
		uint8_t title_scale_w;
		uint8_t title_scale_h;
		uint8_t title_scale;
		uint8_t title_char_w;
		uint8_t title_char_h;
		uint16_t title_block_h;
		uint16_t title_top;
		uint16_t title_y1;
		uint16_t title_y2;
		uint16_t title_y3;

		if ((uint8_t)strlen(title2) > max_title_len) max_title_len = (uint8_t)strlen(title2);
		if ((uint8_t)strlen(title3) > max_title_len) max_title_len = (uint8_t)strlen(title3);
		if (max_title_len == 0) max_title_len = 1;

		title_scale_w = (uint8_t)(TFT_WIDTH / (max_title_len * (8 - 2)));
		if (title_scale_w < 1) title_scale_w = 1;
		if (title_scale_w > 3) title_scale_w = 3;

		title_scale_h = (uint8_t)((TFT_HEIGHT - (title_gap * 2)) / (8 * 3));
		if (title_scale_h < 1) title_scale_h = 1;
		if (title_scale_h > 3) title_scale_h = 3;

		title_scale = (title_scale_w < title_scale_h) ? title_scale_w : title_scale_h;
		title_char_w = (uint8_t)((8 - 2) * title_scale);
		title_char_h = (uint8_t)(8 * title_scale);
		title_block_h = (uint16_t)(title_char_h * 3 + title_gap * 2);
		title_top = (TFT_HEIGHT > title_block_h) ? (uint16_t)((TFT_HEIGHT - title_block_h) / 2) : 0;
		title_y1 = title_top;
		title_y2 = title_top + title_char_h + title_gap;
		title_y3 = title_top + (title_char_h + title_gap) * 2;

		first_flg = 0;
		tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
		tft_set_cursor(0, 0);
		tft_set_color(BLUE);
		tft_set_cursor((TFT_WIDTH - (uint16_t)(strlen(title1) * title_char_w)) / 2, title_y1);
		tft_print(title1, title_scale);
		tft_set_color(RED);
		tft_set_cursor((TFT_WIDTH - (uint16_t)(strlen(title2) * title_char_w)) / 2, title_y2);
		tft_print(title2, title_scale);
		tft_set_cursor((TFT_WIDTH - (uint16_t)(strlen(title3) * title_char_w)) / 2, title_y3);
		tft_set_color(GREEN);
		tft_print(title3, title_scale)	;
	    Delay_Ms( 1000 );	
	}
	tft_set_color(WHITE);

	initGoertzel(speed);
	sampling_period_us = 900000 / GOERTZEL_SAMPLING_FREQUENCY;
	reset_morse_buffers();
	tim1_pwm_init();
#if defined(TFT_ST7739)
	current_line = 0;
	lcdindex = 0;
	for (int i = 0; i < colums; i++){
		line0[i] = 32;
		line1[i] = 32;
		line2[i] = 32;
		line3[i] = 32;
	}
#else
	current_line = 0;
	lcdindex = 0;
	for (int i = 0; i < colums; i++){
		line0[i] = 32;
		line1[i] = 32;
		line2[i] = 32;
	}
#endif
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
		printAscii('A');
		printAscii('R');
	} else if (asciinumber == 2) {			// KN
		printAscii('K');
		printAscii('N');
	} else if (asciinumber == 3) {			// BT
		printAscii('B');
		printAscii('T');
	} else if (asciinumber == 4) {			// VA
		printAscii('V');
		printAscii('A');
	} else if (asciinumber == 7) {			// HH (訂正)
		printAscii('H');
		printAscii('H');
	} else {
		printAscii(asciinumber);
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
	uint8_t draw_div = 0;

	tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
	info_last_buf[0] = '\0';
	info_sep_drawn = 0;
	info_last_wpm = 0xFFFF;
	info_last_sw = 0xFF;
	info_last_speed = -1;

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
		if (++draw_div >= 8) {
			draw_div = 0;
			tft_draw_line(TFT_WIDTH - 1, TFT_HEIGHT - 1, TFT_WIDTH - 1, 0, BLACK);
			{
				int16_t w = (int16_t)(TFT_HEIGHT - 1) - (magnitude / 8);
				if (w < 0) w = 0;
				if (w > (int16_t)(TFT_HEIGHT - 1)) w = (int16_t)(TFT_HEIGHT - 1);
				tft_draw_line(TFT_WIDTH - 1, TFT_HEIGHT - 1, TFT_WIDTH - 1, w, YELLOW);
			}
		}
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
		{
			uint32_t unit = (hightimesavg > 0) ? hightimesavg : highduration;
			nbtime = compute_nbtime(unit);
		}
		if ((millis()-laststarttime)> nbtime) {
			if (realstate != filteredstate) {
				filteredstate = realstate;
			}
		}

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
				hightimesavg = (highduration+hightimesavg+hightimesavg) / 3;     // 短点平均を更新 (3点移動平均)
				}
				if (highduration > (5*hightimesavg) ) {
				hightimesavg = highduration+hightimesavg;     // 速度低下が急な場合に追従
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
		updateinfolinelcd();
		realstatebefore = realstate;
		lasthighduration = highduration;
		filteredstatebefore = filteredstate;
		morseReady[buf_idx] = 0;
	}
	TIM1->DMAINTENR &= ~TIM_IT_Update;
	TIM1->CTLR1 &= ~TIM_CEN;
    return 0;
}
