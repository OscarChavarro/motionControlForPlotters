#ifndef MOTION_STEPPER_MOTOR_H
#define MOTION_STEPPER_MOTOR_H

#include <stdint.h>

enum StepperMotorType {
    BIPOLAR_STEPPER_MOTOR,
    UNIPOLAR_STEPPER_MOTOR
};

enum BiPolarStepperMotorMicroStep {
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_1 = 1,
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_2 = 2,
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_4 = 4,
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_6 = 6,
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_8 = 8,
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_16 = 16,
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_32 = 32,
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_64 = 64,
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_128 = 128,
    BIPOLAR_STEPPER_MOTOR_MICROSTEP_256 = 256
};

class StepperMotor {
  public:
    StepperMotorType type;
    const char* referenceName;
    uint16_t fullStepsPerRotation;
    BiPolarStepperMotorMicroStep microSteps;
    uint8_t stepPin;
    uint8_t directionPin;

    StepperMotor(
        StepperMotorType type,
        const char* referenceName,
        uint16_t fullStepsPerRotation,
        BiPolarStepperMotorMicroStep microSteps,
        uint8_t stepPin,
        uint8_t directionPin);

    uint16_t microStepsPerFullStep() const;
    const char* typeName() const;
    uint32_t microStepsPerRotation() const;
    uint32_t microStepsFromRotations(uint16_t rotations) const;
    uint32_t microStepsPerSecondFromMilliRotationsPerSecond(
        uint32_t milliRotationsPerSecond) const;
    uint32_t milliMicroStepsPerSecondForTrapezoidRotations(
        uint16_t rotations,
        uint16_t accelerationMilliseconds,
        uint16_t cruiseMilliseconds,
        uint16_t decelerationMilliseconds) const;
    uint32_t milliRotationsPerSecondFromMicroStepsPerSecond(
        uint32_t microStepsPerSecond) const;
    uint32_t milliRotationsPerSecondFromMilliMicroStepsPerSecond(
        uint32_t milliMicroStepsPerSecond) const;
    uint32_t milliDegreesPerSecondFromMicroStepsPerSecond(
        uint32_t microStepsPerSecond) const;
    uint32_t milliDegreesPerSecondFromMilliMicroStepsPerSecond(
        uint32_t milliMicroStepsPerSecond) const;
    uint32_t centiDegreesFromMicroStepPosition(int32_t microStepPosition) const;
};

#endif
