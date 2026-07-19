#include "motion/stepperMotor/StepperMotor.h"

StepperMotor::StepperMotor(
    StepperMotorType type,
    const char* referenceName,
    uint16_t fullStepsPerRotation,
    BiPolarStepperMotorMicroStep microSteps,
    uint8_t stepPin,
    uint8_t directionPin)
    : type(type),
      referenceName(referenceName),
      fullStepsPerRotation(fullStepsPerRotation),
      microSteps(microSteps),
      stepPin(stepPin),
      directionPin(directionPin)
{
}

uint16_t
StepperMotor::microStepsPerFullStep() const
{
    return static_cast<uint16_t>(microSteps);
}

const char*
StepperMotor::typeName() const
{
    if (type == BIPOLAR_STEPPER_MOTOR) {
        return "BIPOLAR_STEPPER_MOTOR";
    }
    return "UNIPOLAR_STEPPER_MOTOR";
}

uint32_t
StepperMotor::microStepsPerRotation() const
{
    return static_cast<uint32_t>(fullStepsPerRotation) *
        microStepsPerFullStep();
}

uint32_t
StepperMotor::microStepsFromRotations(uint16_t rotations) const
{
    return microStepsPerRotation() * rotations;
}

uint32_t
StepperMotor::microStepsPerSecondFromMilliRotationsPerSecond(
    uint32_t milliRotationsPerSecond) const
{
    return static_cast<uint32_t>(
        (static_cast<uint64_t>(microStepsPerRotation()) *
            milliRotationsPerSecond) /
        1000ULL);
}

uint32_t
StepperMotor::milliMicroStepsPerSecondForTrapezoidRotations(
    uint16_t rotations,
    uint16_t accelerationMilliseconds,
    uint16_t cruiseMilliseconds,
    uint16_t decelerationMilliseconds) const
{
    const uint32_t effectiveDurationTwiceMilliseconds =
        static_cast<uint32_t>(accelerationMilliseconds) +
        (2UL * cruiseMilliseconds) + decelerationMilliseconds;
    if (effectiveDurationTwiceMilliseconds == 0UL) {
        return 0UL;
    }
    const uint64_t milliMicroStepsPerSecondTwice =
        static_cast<uint64_t>(microStepsFromRotations(rotations)) *
        2000000ULL;
    return static_cast<uint32_t>(
        (milliMicroStepsPerSecondTwice +
            effectiveDurationTwiceMilliseconds - 1ULL) /
        effectiveDurationTwiceMilliseconds);
}

uint32_t
StepperMotor::milliRotationsPerSecondFromMicroStepsPerSecond(
    uint32_t microStepsPerSecond) const
{
    const uint32_t stepsPerRotation = microStepsPerRotation();
    if (stepsPerRotation == 0UL) {
        return 0UL;
    }
    return static_cast<uint32_t>(
        (static_cast<uint64_t>(microStepsPerSecond) * 1000ULL) /
        stepsPerRotation);
}

uint32_t
StepperMotor::milliRotationsPerSecondFromMilliMicroStepsPerSecond(
    uint32_t milliMicroStepsPerSecond) const
{
    const uint32_t stepsPerRotation = microStepsPerRotation();
    if (stepsPerRotation == 0UL) {
        return 0UL;
    }
    return static_cast<uint32_t>(
        static_cast<uint64_t>(milliMicroStepsPerSecond) / stepsPerRotation);
}

uint32_t
StepperMotor::milliDegreesPerSecondFromMicroStepsPerSecond(
    uint32_t microStepsPerSecond) const
{
    const uint32_t stepsPerRotation = microStepsPerRotation();
    if (stepsPerRotation == 0UL) {
        return 0UL;
    }
    return static_cast<uint32_t>(
        (static_cast<uint64_t>(microStepsPerSecond) * 360000ULL) /
        stepsPerRotation);
}

uint32_t
StepperMotor::milliDegreesPerSecondFromMilliMicroStepsPerSecond(
    uint32_t milliMicroStepsPerSecond) const
{
    const uint32_t stepsPerRotation = microStepsPerRotation();
    if (stepsPerRotation == 0UL) {
        return 0UL;
    }
    return static_cast<uint32_t>(
        (static_cast<uint64_t>(milliMicroStepsPerSecond) * 360ULL) /
        stepsPerRotation);
}

uint32_t
StepperMotor::centiDegreesFromMicroStepPosition(
    int32_t microStepPosition) const
{
    const uint32_t stepsPerRotation = microStepsPerRotation();
    if (stepsPerRotation == 0UL) {
        return 0UL;
    }

    uint32_t absolutePosition = 0UL;
    if (microStepPosition < 0L) {
        absolutePosition =
            static_cast<uint32_t>(-(microStepPosition + 1L)) + 1UL;
    }
    else {
        absolutePosition = static_cast<uint32_t>(microStepPosition);
    }

    uint32_t positionInRotation = absolutePosition % stepsPerRotation;
    if (positionInRotation == 0UL && absolutePosition > 0UL) {
        positionInRotation = stepsPerRotation;
    }
    return static_cast<uint32_t>(
        (static_cast<uint64_t>(positionInRotation) * 36000ULL) /
        stepsPerRotation);
}
