#ifndef XY_MOTION_CONTROLLER_SYSTEMCLOCK_H
#define XY_MOTION_CONTROLLER_SYSTEMCLOCK_H

#include <stdint.h>

class SystemClock {
public:
    static void initialize();
    static uint32_t millis();

    static void onTimerCompareMatch();

private:
    static volatile uint32_t g_systemMillis;
    static uint8_t timerCompareValue();
};

#endif
