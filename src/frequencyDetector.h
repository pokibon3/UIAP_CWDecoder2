//
//	Frequency Detector Header
//
#pragma once

#define FD_SAMPLING_FREQUENCY 6000	// Hz	+10%

extern int fd_setup(void);
#if defined(BOARD_CH32V006)
extern int freqDetector(float *vReal, float *vImag);
#else
extern int freqDetector(int8_t *vReal, int8_t *vImag);
#endif
