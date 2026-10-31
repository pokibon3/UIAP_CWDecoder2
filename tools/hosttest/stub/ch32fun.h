#pragma once
#include <string.h>
#include <stdint.h>
struct host_tim { volatile uint16_t INTFR; };
extern struct host_tim host_tim1;
#define TIM1 (&host_tim1)
#define TIM_IT_Update 1
static inline void Delay_Ms(int) {}
