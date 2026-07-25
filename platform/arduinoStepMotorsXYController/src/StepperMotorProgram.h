#ifndef STEPPER_MOTOR_PROGRAM_H
#define STEPPER_MOTOR_PROGRAM_H

#include <stdint.h>

class UartSerial;
class StepperMotor;

#include "motion/stepperMotor/StepperMotor.h"

class StepperMotorProgram {
  public:
    // Sentinel for uartReceivePin/uartTransmitPin when the driver is used
    // in plain STEP/DIR mode (A4988, or a TMC2209 with UART disabled).
    static const uint8_t NO_UART_PIN = 0xFFU;

    StepperMotor motor;
    uint16_t stepPulseMicroseconds;
    uint16_t directionSetupMicroseconds;
    uint16_t travelRotations;
    uint16_t accelerationMilliseconds;
    uint16_t cruiseMilliseconds;
    uint16_t decelerationMilliseconds;
    const char* driverDescription;
    uint8_t uartReceivePin;
    uint8_t uartTransmitPin;

    StepperMotorProgram(
        const StepperMotor& motor,
        uint16_t stepPulseMicroseconds,
        uint16_t directionSetupMicroseconds,
        uint16_t travelRotations,
        uint16_t accelerationMilliseconds,
        uint16_t cruiseMilliseconds,
        uint16_t decelerationMilliseconds,
        const char* driverDescription,
        uint8_t uartReceivePin,
        uint8_t uartTransmitPin);

    void printConfiguration(
        UartSerial& serial,
        uint8_t motorIndex,
        uint32_t maxMilliMicroStepsPerSecond,
        bool motorReady) const;
};

#endif
