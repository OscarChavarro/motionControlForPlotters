#ifndef ARDUINO_STEP_MOTORS_AVR_SYSTEM_CLOCK_H
#define ARDUINO_STEP_MOTORS_AVR_SYSTEM_CLOCK_H

#include "hal/SystemClock.h"

class AvrSystemClock : public SystemClock {
  private:
    static volatile uint32_t g_systemMillis;
    static uint8_t timerCompareValue();

  public:
    ~AvrSystemClock() = default;

    void initialize() override;
    uint32_t millis() const override;

    static void onTimerCompareMatch();
};

#endif
