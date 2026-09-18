// デコード周波数の段数 (600/700/800/900/1000Hz)
#define GOERTZEL_TONES 5
void initGoertzel(int16_t speed);
void setSpeed(int16_t speed);
int32_t goertzel(int16_t *data, int16_t n);
int32_t goertzelSideMag(void);
int32_t goertzelSideMagInst(void);
int32_t goertzelSideMagMax(void);
