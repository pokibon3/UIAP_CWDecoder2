//
//	3-bin integer Goertzel filter (CH32V003: no FPU / no hardware multiply)
//
//	Center bin tracks the target tone; two side bins sit exactly
//	+-2 DFT bins away (+-341.33 Hz @ 8192 Hz / 48 samples), on nulls of
//	the rectangular-window Dirichlet kernel. A clean tone therefore
//	leaks almost nothing into the side bins, while white noise raises
//	all three bins roughly equally - the center/side ratio separates
//	tone from noise.
//
//	Coefficients are Q14 fixed point (shift instead of divide), which
//	is both faster and more precise than the old coeff100 scheme.
//
//	Two side levels are reported:
//	- goertzelSideMag()     = min(EMA(low), EMA(high)). min() keeps a
//	  one-sided interferer (QRM) from masking the tone; the per-side
//	  EMA (alpha = 1/4, ~4 blocks = 23 ms) removes the block-to-block
//	  variance of white noise.
//	- goertzelSideMagInst() = min(low, high) of the current block only,
//	  used by the decoder when turning ON from silence to reject
//	  broadband impulses that the EMA has not caught up with yet.
//
#if !defined(BOARD_CH32V006)

#include <stdint.h>

// coeff_q14 = round(2 * cos(2 * pi * f / 8192) * 16384)
static const int32_t coeff_tbl[3][3] = {
	// center   low(-341.3Hz) high(+341.3Hz)
	{ 28576, 31753, 23453 },  // 666.7 Hz (sides 325.4 / 1008.0)
	{ 26300, 30463, 20345 },  // 833.3 Hz (sides 492.0 / 1174.6)
	{ 23593, 28675, 16904 },  // 1000 Hz (sides 658.7 / 1341.3)
};

static int32_t coeff_c = 28576;
static int32_t coeff_l = 31753;
static int32_t coeff_h = 23453;

static int32_t side_mag = 0;
static int32_t side_mag_inst = 0;
static int32_t side_mag_max = 0;
static int32_t side_ema_l = 0;
static int32_t side_ema_h = 0;
static uint8_t side_ema_started = 0;

void setSpeed(int16_t speed)
{
    if (speed < 0 || speed > 2) {
        speed = 0;
    }
    coeff_c = coeff_tbl[speed][0];
    coeff_l = coeff_tbl[speed][1];
    coeff_h = coeff_tbl[speed][2];
}

void initGoertzel(int16_t speed)
{
    setSpeed(speed);
    side_mag = 0;
    side_mag_inst = 0;
    side_mag_max = 0;
    side_ema_l = 0;
    side_ema_h = 0;
    side_ema_started = 0;
}

// floor(sqrt(x))
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

static int32_t goertzel_mag(int32_t q1, int32_t q2, int32_t coeff)
{
    // scale down before squaring to stay inside int32
    q1 >>= 2;
    q2 >>= 2;
    int32_t m2 = q1 * q1 + q2 * q2 - (int32_t)(((int64_t)q1 * q2 * coeff) >> 14);
    if (m2 < 0) {
        m2 = 0;
    }
    return (int32_t)isqrt32((uint32_t)m2) << 2;
}

int32_t goertzel(int16_t *data, int16_t n)
{
    int32_t q1c = 0, q2c = 0;
    int32_t q1l = 0, q2l = 0;
    int32_t q1h = 0, q2h = 0;

    for (int16_t i = 0; i < n; i++) {
        const int32_t x = data[i];
        int32_t q0;
        q0 = ((coeff_c * q1c) >> 14) - q2c + x; q2c = q1c; q1c = q0;
        q0 = ((coeff_l * q1l) >> 14) - q2l + x; q2l = q1l; q1l = q0;
        q0 = ((coeff_h * q1h) >> 14) - q2h + x; q2h = q1h; q1h = q0;
    }

    const int32_t mag_l = goertzel_mag(q1l, q2l, coeff_l);
    const int32_t mag_h = goertzel_mag(q1h, q2h, coeff_h);

    side_mag_inst = (mag_l < mag_h) ? mag_l : mag_h;

    if (!side_ema_started) {
        side_ema_started = 1;
        side_ema_l = mag_l;
        side_ema_h = mag_h;
    } else {
        side_ema_l += (mag_l - side_ema_l) / 4;
        side_ema_h += (mag_h - side_ema_h) / 4;
    }
    // 比率判定用サイド: min(L,H) ではなく幾何平均 sqrt(L*H)。
    // 低域から通過帯域へ裾を引く傾斜ノイズでは、min が静かな側だけを
    // 見て帯域内ノイズを過小評価し偽符号が出る。トーン受信時は両サイド
    // とも静粛で min とほぼ同値なので感度は落ちない。
    // sqrt(L)*sqrt(H) == sqrt(L*H): 32bit積のオーバーフローを避ける。
    side_mag = (int32_t)(isqrt32((uint32_t)side_ema_l) * isqrt32((uint32_t)side_ema_h));
    {
        int32_t inst_max = (mag_l > mag_h) ? mag_l : mag_h;
        int32_t ema_max = (side_ema_l > side_ema_h) ? side_ema_l : side_ema_h;
        side_mag_max = (inst_max > ema_max) ? inst_max : ema_max;
    }

    return goertzel_mag(q1c, q2c, coeff_c);
}

// Smoothed min side-bin magnitude as of the last goertzel() call.
int32_t goertzelSideMag(void)
{
    return side_mag;
}

// Instantaneous (unsmoothed) min side-bin magnitude of the last block.
int32_t goertzelSideMagInst(void)
{
    return side_mag_inst;
}

// Max side level: max of both EMAs and both instantaneous side mags.
// Used to reject out-of-band noise leaking into the center bin.
int32_t goertzelSideMagMax(void)
{
    return side_mag_max;
}

#endif
