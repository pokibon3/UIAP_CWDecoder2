//
//	Frequency Detector Header
//
#pragma once

#define FD_SAMPLING_FREQUENCY 6000	// Hz	+10%

extern int fd_setup(void);

// CH32V006: float FFT バッファを受け取る
// CH32V003: 既存の int8_t バッファを受け取る (変更なし)
#if defined(BOARD_CH32V006)
extern int freqDetector(float *vReal, float *vImag);
#else
extern int freqDetector(int8_t *vReal, int8_t *vImag);
#endif
