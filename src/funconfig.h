//
//	CH32V Fun Config Header
//
#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

#define FUNCONF_USE_HSI 1    		// internal 24MHz clock oscillator
#define FUNCONF_USE_PLL 1			// use PLL x2
#define FUNCONF_SYSTEM_CORE_CLOCK 48000000

#define FUNCONF_USE_DEBUGPRINTF 0
#define FUNCONF_USE_UARTPRINTF  1
#define FUNCONF_UART_PRINTF_BAUD 115200

// チップ種別は board の extra_flags (-DCH32V003 / -DCH32V006 等) で定義されるため
// ここでは重複定義しない。

// ADC サンプルタイム (ch32v003_GPIO_branchless.h 向け)
#define GPIO_ADC_sampletime GPIO_ADC_sampletime_43cy

// ================================================================
// CH32V006 向け互換定義
//   ch32x00xhw.h はレジスタビットをタイマー毎にプレフィックス付きで定義する。
//   ch32v003_GPIO_branchless.h がコンパイルできるよう汎用名へエイリアスを張る。
//   (インライン関数は未使用でもコンパイルされるため、定義が必要)
// ================================================================
#if defined(CH32V006) || defined(CH32V00x)

// ADC calibration: ch32v003hw.h の ADC_RSTCAL/ADC_CAL を ch32x00xhw.h のマクロへ
#define ADC_RSTCAL  CTLR2_RSTCAL_Set
#define ADC_CAL     CTLR2_CAL_Set

// RCC 構造体メンバ名: V006 では APB1PRSTR → PB1PRSTR
#define APB1PRSTR   PB1PRSTR

// TIM1 ビットフィールド: V006 では TIM1_ プレフィックスが付く
#define TIM_CEN      TIM1_CTLR1_CEN
#define TIM_ARPE     TIM1_CTLR1_ARPE
#define TIM_UG       TIM1_SWEVGR_UG
#define TIM_MOE      TIM1_BDTR_MOE
#define TIM_CC1E     TIM1_CCER_CC1E
#define TIM_OC1M_1   TIM1_CHCTLR1_OC1M_1
#define TIM_OC1M_2   TIM1_CHCTLR1_OC1M_2
#define TIM_OC1PE    TIM1_CHCTLR1_OC1PE

#endif // CH32V006 || CH32V00x

#endif // _FUNCONFIG_H
