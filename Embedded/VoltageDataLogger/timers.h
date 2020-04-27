#ifndef TIMERS_H
#define TIMERS_H
#include "MKL25Z4.h"

#define PWM_PERIOD (48000)
#define FULL_ON (PWM_PERIOD-1)
#define FULL_OFF (0)

void Init_Timer_Output(uint16_t period);

#endif

