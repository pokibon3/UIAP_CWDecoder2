// Float FFT — CH32V006 のみコンパイル
// CH32V003 ビルドでは全コードが除外される
#if defined(BOARD_CH32V006)

#include "float_fft.h"
#include <math.h>

//
// 前向き FFT (Cooley-Tukey, decimation-in-time, in-place)
//
// ・sin/cos はステージごとに 1 回だけ計算 (log2n 回) し、
//   twiddle を逐次回転することで呼び出しコストを最小化。
// ・ROM: 関数コード + soft-float ランタイム
// ・RAM: ローカル変数のみ (追加静的領域なし)
//
void float_fft(float *fr, float *fi, int log2n)
{
    const int n = 1 << log2n;

    // ---- ビット反転並び替え (bit-reversal permutation) ----
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t;
            t = fr[i]; fr[i] = fr[j]; fr[j] = t;
            t = fi[i]; fi[i] = fi[j]; fi[j] = t;
        }
    }

    // ---- バタフライ演算 (log2n ステージ) ----
    // 各ステージで sin/cos を 1 回だけ計算し、twiddle を乗算で逐次更新する。
    for (int stage = 1; stage <= log2n; stage++) {
        const int m  = 1 << stage;   // バタフライ幅
        const int mh = m >> 1;       // 半幅

        // twiddle の基本角度: -2π/m  (前向き FFT は負の指数)
        const float ang = -(float)M_PI / (float)mh;
        const float wR = cosf(ang);
        const float wI = sinf(ang);

        for (int k = 0; k < n; k += m) {
            float cR = 1.0f, cI = 0.0f;   // 累積 twiddle
            for (int j = 0; j < mh; j++) {
                // バタフライ
                const float uR = fr[k + j];
                const float uI = fi[k + j];
                const float vR = fr[k + j + mh] * cR - fi[k + j + mh] * cI;
                const float vI = fr[k + j + mh] * cI + fi[k + j + mh] * cR;
                fr[k + j]       = uR + vR;
                fi[k + j]       = uI + vI;
                fr[k + j + mh]  = uR - vR;
                fi[k + j + mh]  = uI - vI;
                // twiddle を次の j 用に回転
                const float tmp = cR * wR - cI * wI;
                cI = cR * wI + cI * wR;
                cR = tmp;
            }
        }
    }
}

#endif // BOARD_CH32V006
