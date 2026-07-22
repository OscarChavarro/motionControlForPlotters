#ifndef STEPPER_MOTOR_PROGRAM_H
#define STEPPER_MOTOR_PROGRAM_H

#include <stdint.h>

class UartSerial;
class StepperMotor;

#include "motion/stepperMotor/StepperMotor.h"

class StepperMotorProgram {
  public:
    StepperMotor motor;
    uint16_t stepPulseMicroseconds;
    uint16_t directionSetupMicroseconds;
    uint16_t travelRotations;
    uint16_t accelerationMilliseconds;
    uint16_t cruiseMilliseconds;
    uint16_t decelerationMilliseconds;

    StepperMotorProgram(
        const StepperMotor& motor,
        uint16_t stepPulseMicroseconds,
        uint16_t directionSetupMicroseconds,
        uint16_t travelRotations,
        uint16_t accelerationMilliseconds,
        uint16_t cruiseMilliseconds,
        uint16_t decelerationMilliseconds);

    void printConfiguration(
        UartSerial& serial,
        uint8_t motorIndex,
        uint32_t maxMilliMicroStepsPerSecond,
        bool motorReady) const;
};

#endif
