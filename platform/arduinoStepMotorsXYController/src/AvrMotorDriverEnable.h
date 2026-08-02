#ifndef ARDUINO_STEP_MOTORS_AVR_MOTOR_DRIVER_ENABLE_H
#define ARDUINO_STEP_MOTORS_AVR_MOTOR_DRIVER_ENABLE_H

#include "hal/MotorDriverEnable.h"

#include <stdint.h>

class AvrMotorDriverEnable : public MotorDriverEnable {
  private:
    volatile uint8_t* m_outputRegister;
    uint8_t m_bitMask;

  public:
    AvrMotorDriverEnable();

    void initialize() override;
    void setEnabled(bool enabled) override;
};

#endif
