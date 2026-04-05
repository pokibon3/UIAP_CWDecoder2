#pragma once
// Float FFT — CH32V006 のみ使用
// CH32V003 ビルドではこのヘッダを include しないこと
#if defined(BOARD_CH32V006)

// 前向き FFT (in-place, Cooley-Tukey DIT)
//   fr[n], fi[n] : 実部/虚部 (入力 & 出力)
//   log2n        : log2(サンプル数)  例: 128サンプル → 7
void float_fft(float *fr, float *fi, int log2n);

#endif // BOARD_CH32V006
