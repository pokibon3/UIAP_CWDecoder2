//
//	CWデコーダ関数
//
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "goertzel.h"
#include "decode.h"
#include "ch32fun.h"
#include "cw_display.h"

//#define SERIAL_OUT

#define GOERTZEL_SAMPLES 48
#define GOERTZEL_SAMPLING_FREQUENCY 8192
#define NOISE_BLANKER_ENABLED 1
// トーン判定: 中心ビンがサイドレベル(±341.33Hzビンの平滑化後min)の
// 何倍あれば正弦波とみなすか。ホワイトノイズは全ビンほぼ同レベル、
// 正弦波は10倍以上になる。サイドは幾何平均√(L×H)を使用 (v2.0):
// min(L,H)だと低域から裾を引く傾斜ノイズが静かな側との比較をすり抜ける。
#define TONE_SIDE_RATIO 3
// トーン判定のヒステリシス(シュミットトリガ):
// ON  = 振幅 0.6×limit 超 かつ 中心 > 3×サイド
// OFF = 振幅 0.4×limit 未満 または 中心 < 2.5×サイド
// 中間帯は前状態を保持し、しきい値付近のバタつきでマークが千切れるのを防ぐ。
// (OFF をこれ以上緩めるとホワイトノイズの保持が伸びて誤記号が出る)
#define TONE_SIDE_RATIO_OFF_X10 25
// 実運用の中心速度帯 20〜35wpm の単位長範囲 (ms、マージン込み)。
// 短点+長点ペアで求めた単位長がこの範囲なら速度推定を即時スナップする。
#define WPM_CORE_UNIT_MIN 30
#define WPM_CORE_UNIT_MAX 66
static int32_t magnitudelimit = 140;
static int32_t magnitudelimit_low = 140;
// 振幅しきい値 (magnitudelimit) の追従 (v2.1): 立ち上がりは速く (1/6 = τ35ms)、
// 下降は遅く (1/64 = τ0.38s) してピークホールド的に振る舞わせる。
// 旧来の対称 1/6 では文字間ギャップで limit がノイズ床まで崩落して振幅条件が
// 無効化され、絶対床 140 が「入力を極端に絞ったときだけ」スケルチとして
// 働いていた (ノイズ混じりは音量をかなり下げないとデコードできなかった)。
#define LIMIT_ATTACK_DIV 6
#define LIMIT_DECAY_DIV 64
// ノイズ床に連動した相対スケルチ (v2.1): トーンらしくないブロックの中心
// マグニチュードを遅い EMA (1/128 = τ0.75s) で追い、limit の床を
// noise_floor x 6 にする (ON 下限 = 0.6 x 床 = ノイズ平均 x 3.6)。
// EMA は Q8 で蓄積する (整数 EMA は小さな差で動かず片道ラチェットになる)。
// (ESP32版 cw_decoder4 v2.1/v2.2 の対策を移植。同版は実機ログで x8 に
//  上げているが、窓条件が異なるため x6 から調整する)
static int32_t noise_floor = 0;
static int32_t noise_acc = 0;       // noise_floor の Q8 蓄積値
#define NOISE_FLOOR_DIV 128
#define NOISE_SQUELCH_X10 60
static uint16_t realstate = GPIO_LOW;
static uint16_t realstatebefore = GPIO_LOW;
static uint16_t filteredstate = GPIO_LOW;
static uint16_t filteredstatebefore = GPIO_LOW;
static uint32_t starttimehigh;
static uint32_t highduration;
static uint32_t lasthighduration;
static uint32_t hightimesavg = 60;
static uint32_t startttimelow;
static uint32_t lowduration;
// ノイズブランカの積分値 (0..深さ)
static uint8_t nb_acc = 0;
// 1ブロック = 48/8192Hz = 5.86ms。深さは 0.35 単位ぶんのブロック数 (四捨五入)
// = unit[ms] x 0.35 / 5.86 ≒ unit x 6 / 100。上限 3 ブロック (17.6ms) は
// 推定が遅い側へ外れても 50WPM の短点 (24ms) を残すための保険
#define NB_DEPTH_MIN 2
#define NB_DEPTH_MAX 3

static char code[20];
static uint16_t stop = GPIO_LOW;
static uint16_t wpm;
static uint32_t last_mark_ms = 0;
static uint32_t last_gap_ms = 0;

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

static void push_morse_sample(uint16_t sample)
{
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

static inline int32_t normalize_decoder_magnitude(int32_t magnitude)
{
#if defined(BOARD_CH32V006)
	return magnitude >> 2;
#else
	return magnitude;
#endif
}

extern "C" void TIM1_UP_IRQHandler(void) __attribute__((interrupt));
extern "C" void TIM1_UP_IRQHandler(void)
{
	if (TIM1->INTFR & TIM_IT_Update) {
		TIM1->INTFR = (uint16_t)~TIM_IT_Update;
	}

	push_morse_sample((uint16_t)(adc_read_raw() >> 1));
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
//	単位長(短点)の推定ヘルパー
//==================================================================
// 比率~1:3のペア(短点+長点、または短点+文字間ギャップ等)から単位長候補を
// 求める。20〜35wpm帯のみ有効。同じ長さのペアは「短点2つ」とも「長点2つ」
// とも解釈できて曖昧なため使わない(低速CWを壊さないための安全条件)。
static uint32_t snap_candidate(uint32_t a, uint32_t b)
{
	uint32_t hi = (a > b) ? a : b;
	uint32_t lo = (a > b) ? b : a;
	if ((hi * 10) >= (lo * 24) && (hi * 10) <= (lo * 36)) {
		uint32_t cu = (lo + hi / 3) / 2;
		if (cu >= WPM_CORE_UNIT_MIN && cu <= WPM_CORE_UNIT_MAX) {
			return cu;
		}
	}
	return 0;
}

static void unit_clamp(void)
{
	if (hightimesavg < 24) {
		hightimesavg = 24;
	}
	if (hightimesavg > 300) {
		hightimesavg = 300;
	}
}

// スナップ: 現在値から大きくズレていれば即時復帰、近ければ平滑化
static void unit_apply_snap(uint32_t snap)
{
	uint32_t diff = (snap > hightimesavg) ? (snap - hightimesavg)
	                                      : (hightimesavg - snap);
	if (diff * 4 > hightimesavg) {
		hightimesavg = snap;
	} else if (snap >= hightimesavg) {
		hightimesavg += (snap - hightimesavg) / 3;
	} else {
		hightimesavg -= (hightimesavg - snap) / 3;
	}
	unit_clamp();
}

// 緩やか追従: 2単位未満は1単位(そのまま)、以上は3単位(1/3)として反映。
// 低速側の推定値は現在値の2倍でクリップし、持続ノイズバーストで
// 一気に低速側へ倒れないよう変化率を制限する。
static void unit_smooth_update(uint32_t dur)
{
	uint32_t est = dur;
	if (dur >= (2 * hightimesavg) && hightimesavg != 0) {
		est = dur / 3;
		if (est > hightimesavg * 2) {
			est = hightimesavg * 2;
		}
	}
	if (est >= hightimesavg) {
		hightimesavg += (est - hightimesavg) / 3;
	} else {
		hightimesavg -= (hightimesavg - est) / 3;
	}
	unit_clamp();
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
		if (speed >= GOERTZEL_TONES) speed = 0;
		setSpeed(speed);
		cw_display_update_info(wpm, (uint8_t)sw, speed);
		cw_display_tick();
		Delay_Ms(300);
	} else if (val == 2) {
		sw ^= 1;
		cw_display_update_info(wpm, (uint8_t)sw, speed);
		cw_display_tick();
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
	magnitudelimit = magnitudelimit_low;
	noise_floor = 0;
	noise_acc = 0;
	nb_acc = 0;
	sampling_period_us = 900000 / GOERTZEL_SAMPLING_FREQUENCY;
	reset_morse_buffers();
	wpm = 0;
	cw_display_update_info(wpm, (uint8_t)sw, speed);
	cw_display_tick();
	tim1_pwm_init();
	return 0;
}


//==================================================================
//	cwDecoder : デコーダ本体
//==================================================================
static int decodeAscii(int16_t asciinumber);

static void decode_and_display(void)
{
	if (strlen(code) == 0) return;
	int16_t result = decode(code, &sw);
	if (result == 0) {
		cw_display_enqueue_char('*');
	} else {
		decodeAscii(result);
	}
	code[0] = '\0';
}

static int decodeAscii(int16_t asciinumber)
{
	if (asciinumber == 0) return 0;
	if (lastChar == 32 && asciinumber == 32) return 0;

	if        (asciinumber == 1) {			// AR
		cw_display_enqueue_char('A');
		cw_display_enqueue_char('R');
	} else if (asciinumber == 2) {			// KN
		cw_display_enqueue_char('K');
		cw_display_enqueue_char('N');
	} else if (asciinumber == 3) {			// BT
		cw_display_enqueue_char('B');
		cw_display_enqueue_char('T');
	} else if (asciinumber == 4) {			// VA
		cw_display_enqueue_char('V');
		cw_display_enqueue_char('A');
	} else if (asciinumber == 7) {			// HH (訂正)
		cw_display_enqueue_char('H');
		cw_display_enqueue_char('H');
	} else {
		cw_display_enqueue_char(asciinumber);
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
	cw_display_update_info(wpm, (uint8_t)sw, speed);
	cw_display_tick();

	while(1) {
		if (check_sw() == 1) {
			break;
		}
		if (!morseReady[0] && !morseReady[1]) {
			cw_display_tick();
			continue;
		}

		int buf_idx = morseReady[0] ? 0 : 1;
		int16_t *morseData = get_morse_buf((uint8_t)buf_idx);
		int16_t ave = (int16_t)(morseSum[buf_idx] / GOERTZEL_SAMPLES);
		for (int i = 0; i < GOERTZEL_SAMPLES; i++) {
			morseData[i] -= ave;
		}

		// Goertzel 計算 (中心 + サイド2ビン)
		magnitude = goertzel(morseData, GOERTZEL_SAMPLES);
		magnitude = normalize_decoder_magnitude(magnitude);
		int32_t side_mag = normalize_decoder_magnitude(goertzelSideMag());
		int32_t side_max = normalize_decoder_magnitude(goertzelSideMagMax());
		// 立ち上がり(現在LOW)時のみ瞬時サイドも見る:
		// 広帯域インパルスはEMAが追従する前の1ブロック目をすり抜けるため。
		if (filteredstate == GPIO_LOW) {
			int32_t side_inst = normalize_decoder_magnitude(goertzelSideMagInst());
			if (side_inst > side_mag) {
				side_mag = side_inst;
			}
		}
//TEST_LOW
		cw_display_draw_magnitude(magnitude);
#ifdef SERIAL_OUT
		printf("mag = %d\n", magnitude);
#endif
		///////////////////////////////////////////////////////////
		// 振幅しきい値を自動更新
		///////////////////////////////////////////////////////////
		// ノイズ床の更新。判定状態で選別すると ON に失敗した信号が床に
		// 取り込まれて二度と ON にならない (正帰還) ので、ブロックの性質で
		// 選別する: トーンらしくない (中心 <= サイド x 2) か、床未満のブロック
		// だけを取り込む。CW のマークは狭帯域なので除外され、ノイズが本当に
		// 増えたときは広帯域なので追従する。
		{
			uint8_t tone_like = (magnitude > side_mag * 2);
			if (noise_acc == 0) {
				noise_acc = magnitude << 8;
			} else if (!tone_like || magnitude < noise_floor) {
				noise_acc += ((magnitude << 8) - noise_acc) / NOISE_FLOOR_DIV;
			}
			noise_floor = noise_acc >> 8;
		}
		int32_t limit_floor = noise_floor * NOISE_SQUELCH_X10 / 10;
		if (limit_floor < magnitudelimit_low) {
			limit_floor = magnitudelimit_low;
		}
		// 立ち上がり速く、下降は遅く。床はノイズ連動
  		if (magnitude > magnitudelimit_low) {
			int32_t d = magnitude - magnitudelimit;
			magnitudelimit += (d > 0) ? d / LIMIT_ATTACK_DIV : d / LIMIT_DECAY_DIV;
  		}
  		if (magnitudelimit < limit_floor) {
			magnitudelimit = limit_floor;
		}

		////////////////////////////////////
		// 振幅しきい値 + 中心/サイド比でトーン判定 (ヒステリシス付き)
		// (振幅が十分でも、サイドビンとの比が小さければノイズとして棄却。
		//  ON/OFF のしきい値を分け、中間帯は前状態保持でバタつきを抑える)
		////////////////////////////////////
		// tone_on には「中心が両サイドの max (EMA/瞬時とも) を超える」条件も課す:
		// 帯域外ノイズが片サイドだけを上げつつ中心へ漏れるケースを拒否。
		// 本物のトーンは±100Hz程度ズレていても中心が両サイドより必ず大きい。
		{
			uint8_t tone_on  = (((uint32_t)magnitude * 5U) > ((uint32_t)magnitudelimit * 3U)) &&  // 0.6x
			                   (magnitude > side_mag * TONE_SIDE_RATIO) &&
			                   (magnitude > side_max);
			uint8_t tone_off = (((uint32_t)magnitude * 5U) < ((uint32_t)magnitudelimit * 2U)) || // 0.4x
			                   (magnitude * 10 < side_mag * TONE_SIDE_RATIO_OFF_X10);
			if (tone_on) {
				realstate = GPIO_HIGH;
			} else if (tone_off) {
				realstate = GPIO_LOW;
			}
			// 中間帯: realstate は前ブロックの値を保持
		}

		/////////////////////////////////////////////////////
		// ノイズブランカ (v2.1): realstate を上下カウンタで積分する。
		// 旧「nbtime ms 安定したら反映」方式は、しきい値付近で realstate が
		// ブロック毎に往復するとタイマーが毎回リセットされて反映されず、
		// ギャップが埋まって短点 2 つが長点 1 本に融合していた (S→R)。
		// 積分方式なら往復しても多数決で収束し、短いパルスも潰せる。
		// 立ち上がり/立ち下がりを同じ段数だけ遅らせるので要素長は保たれる
		// (長い無音後の即時 ON の特例は、その要素だけ長く測れるので廃止)。
		/////////////////////////////////////////////////////
#if NOISE_BLANKER_ENABLED
		{
			uint32_t unit = (hightimesavg > 0) ? hightimesavg : highduration;
			uint32_t n = (unit * 6 + 50) / 100;
			if (n < NB_DEPTH_MIN) n = NB_DEPTH_MIN;
			if (n > NB_DEPTH_MAX) n = NB_DEPTH_MAX;
			if (realstate == GPIO_HIGH) {
				if (nb_acc < (uint8_t)n) nb_acc++;
			} else if (nb_acc > 0) {
				nb_acc--;
			}
			if (nb_acc >= (uint8_t)n) {
				filteredstate = GPIO_HIGH;
			} else if (nb_acc == 0) {
				filteredstate = GPIO_LOW;
			}
		}
#else
		filteredstate = realstate;
#endif

		////////////////////////////////////////////////////////////
		// HIGH/LOW の継続時間を計測
		////////////////////////////////////////////////////////////
		if (filteredstate != filteredstatebefore) {
			if (filteredstate == GPIO_HIGH) {
				starttimehigh = millis();
				lowduration = (millis() - startttimelow);
				// ギャップ(1単位=文字内 / 3単位=文字間)も速度推定の情報源に
				// する。マークと同数以上あるため収束が速くなる。語間(5単位
				// 以上)は打鍵者の間合いに左右されるため使わない。
				if (lowduration >= 20) {
					if (lowduration < 5 * hightimesavg) {
						uint32_t snap = (last_mark_ms >= 20)
							? snap_candidate(lowduration, last_mark_ms) : 0;
						if (snap != 0) {
							unit_apply_snap(snap);
						} else {
							unit_smooth_update(lowduration);
						}
					}
					last_gap_ms = lowduration;
				}
			}
			if (filteredstate == GPIO_LOW) {
				startttimelow = millis();
				highduration = (millis() - starttimehigh);
				// 単位長(短点)の推定: 20ms未満のマークはノイズとみなし使わない。
				// 直前マーク/直前ギャップとの比率が約1:3なら単位長を一意に
				// 決めて即時スナップ(20〜35wpm帯)、それ以外は緩やかに追従。
				if (highduration >= 20) {
					uint32_t snap = 0;
					if (last_mark_ms >= 20) {
						snap = snap_candidate(highduration, last_mark_ms);
					}
					if (snap == 0 && last_gap_ms >= 20) {
						snap = snap_candidate(highduration, last_gap_ms);
					}
					if (snap != 0) {
						unit_apply_snap(snap);
					} else {
						unit_smooth_update(highduration);
					}
					wpm = (uint16_t)((1200 + hightimesavg / 2) / hightimesavg);
					if (wpm > 50) {
						wpm = 50;
					}
					last_mark_ms = highduration;
				}
			}
		}

		///////////////////////////////////////////////////////////////
		// 短点/長点判定と休止(1/3/7単位)の判定
		// 1/3/7 単位の休止を判定
		// hightimesavg を 1単位(短点)とみなす
		///////////////////////////////////////////////////////////////
		if (filteredstate != filteredstatebefore){
			stop = GPIO_LOW;
			if (filteredstate == GPIO_LOW){  //// HIGH 終了
				if (highduration < (hightimesavg*2) && ((uint32_t)highduration * 5U) > ((uint32_t)hightimesavg * 3U)){ /// 0.6 未満はノイズ除外
					if (strlen(code) >= 8) { decode_and_display(); }
					strcat(code,".");
//					printf(".");
				}
				if (highduration > (hightimesavg*2) && highduration < (hightimesavg*6)){
					if (strlen(code) >= 8) { decode_and_display(); }
					strcat(code,"-");
//					printf("-");
				}
			}
		}
		if (filteredstate == GPIO_HIGH) {  //// LOW 終了

			if (hightimesavg > 0) {
				gap_type_t g = classify_gap(lowduration, hightimesavg);

				if (g == GAP_CHAR) {          // 文字間
					decode_and_display();
				} else if (g == GAP_WORD) {   // 単語間
					decode_and_display();
					decodeAscii(32);           // スペース出力
				}
			}
		}

		//////////////////////////////
		// 一定時間無音なら確定出力
		//////////////////////////////
		uint32_t unit = (hightimesavg > 0) ? hightimesavg : highduration;
		if ((millis() - startttimelow) > unit * 6 && stop == GPIO_LOW) {
			decode_and_display();
			stop = GPIO_HIGH;
		}

		/////////////////////////////////////
		// LED の点灯/消灯
		// スピーカ制御(未使用)
		/////////////////////////////////////
		if(filteredstate == GPIO_HIGH){
				gpio_write_led(GPIO_HIGH);
		} else {
				gpio_write_led(GPIO_LOW);
		}

		//////////////////////////////////
		// ループ終端の状態更新
		/////////////////////////////////
		cw_display_update_info(wpm, (uint8_t)sw, speed);
		cw_display_tick();
		realstatebefore = realstate;
		lasthighduration = highduration;
		filteredstatebefore = filteredstate;
		morseReady[buf_idx] = 0;
	}
	tim1_pwm_stop();
    return 0;
}
