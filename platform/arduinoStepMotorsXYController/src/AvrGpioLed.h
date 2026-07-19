#ifndef ARDUINO_STEP_MOTORS_AVR_GPIO_LED_H
#define ARDUINO_STEP_MOTORS_AVR_GPIO_LED_H

#include "hal/GpioLed.h"

class AvrGpioLed : public GpioLed {
  private:
    static volatile unsigned char* ledPort();
    static volatile unsigned char* ledDdr();
    static unsigned char ledBitMask();

  public:
    ~AvrGpioLed() = default;

    void initialize() override;
    void write(bool enabled) override;
};

#endif
