#ifndef ARDUINO_STEP_MOTORS_AVR_STEP_DIRECTION_DRIVER_H
#define ARDUINO_STEP_MOTORS_AVR_STEP_DIRECTION_DRIVER_H

#include "motion/stepperMotor/StepperMotorDriver.h"

#include <stdint.h>

class AvrStepDirectionDriver : public StepperMotorDriver {
  private:
    static volatile uint8_t* outputRegisterForPin(uint8_t pin);
    static volatile uint8_t* directionRegisterForPin(uint8_t pin);
    static uint8_t bitMaskForPin(uint8_t pin);
    static void delayMicroseconds(uint16_t microseconds);
    static void writePin(
        volatile uint8_t* outputRegister,
        uint8_t bitMask,
        bool high);

    uint8_t m_stepPin;
    uint8_t m_directionPin;
    uint16_t m_stepPulseMicroseconds;
    uint16_t m_directionSetupMicroseconds;
    volatile uint8_t* m_stepOutputRegister;
    volatile uint8_t* m_directionOutputRegister;
    uint8_t m_stepBitMask;
    uint8_t m_directionBitMask;

  public:
    AvrStepDirectionDriver();

    void configure(
        uint8_t stepPin,
        uint8_t directionPin,
        uint16_t stepPulseMicroseconds,
        uint16_t directionSetupMicroseconds);

    bool initialize() override;
    void setDirection(bool forward) override;
    void pulseStep() override;
};

#endif
