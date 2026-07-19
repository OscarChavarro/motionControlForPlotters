#ifndef MOTION_CONTROL_HAL_SYSTEM_CLOCK_H
#define MOTION_CONTROL_HAL_SYSTEM_CLOCK_H

#include <stdint.h>

class SystemClock {
  protected:
    ~SystemClock() = default;

  public:
    virtual void initialize() = 0;
    virtual uint32_t millis() const = 0;
};

#endif
