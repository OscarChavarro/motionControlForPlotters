#include "StepperMotorProgram.h"

#include "hal/UartSerial.h"

StepperMotorProgram::StepperMotorProgram(
    const StepperMotor& motorValue,
    uint16_t stepPulseMicrosecondsValue,
    uint16_t directionSetupMicrosecondsValue,
    uint16_t travelRotationsValue,
    uint16_t accelerationMillisecondsValue,
    uint16_t cruiseMillisecondsValue,
    uint16_t decelerationMillisecondsValue)
    : motor(motorValue),
      stepPulseMicroseconds(stepPulseMicrosecondsValue),
      directionSetupMicroseconds(directionSetupMicrosecondsValue),
      travelRotations(travelRotationsValue),
      accelerationMilliseconds(accelerationMillisecondsValue),
      cruiseMilliseconds(cruiseMillisecondsValue),
      decelerationMilliseconds(decelerationMillisecondsValue)
{
}

void
StepperMotorProgram::printConfiguration(
    UartSerial& serial,
    uint8_t motorIndex,
    uint32_t maxMilliMicroStepsPerSecond,
    bool motorReady) const
{
    serial.writeString("Stepper motor ");
    serial.writeUnsigned(motorIndex);
    serial.writeString(": referenceName=");
    serial.writeString(motor.referenceName);
    serial.writeString(" type=");
    serial.writeString(motor.typeName());
    serial.writeString(" fullSteps/rotation=");
    serial.writeUnsigned(motor.fullStepsPerRotation);
    serial.writeString(" microsteps=");
    serial.writeUnsigned(motor.microStepsPerFullStep());
    serial.writeString(" microsteps/rotation=");
    serial.writeUnsigned(motor.microStepsPerRotation());
    serial.writeString(" stepPin=");
    serial.writeUnsigned(motor.stepPin);
    serial.writeString(" directionPin=");
    serial.writeUnsigned(motor.directionPin);
    serial.writeLine("");

    serial.writeString("Stepper motor ");
    serial.writeUnsigned(motorIndex);
    serial.writeString(" control=");
    serial.writeString(motorReady ? "READY" : "INVALID_PIN_CONFIGURATION");
    serial.writeString(" movement=forward ");
    serial.writeUnsigned(travelRotations);
    serial.writeString(" rotations/");
    serial.writeUnsignedFixed3(
        static_cast<uint32_t>(accelerationMilliseconds) +
        cruiseMilliseconds + decelerationMilliseconds);
    serial.writeString("s, backward ");
    serial.writeUnsigned(travelRotations);
    serial.writeString(" rotations maxSpeed=");
    serial.writeUnsignedFixed3(
        motor.milliRotationsPerSecondFromMilliMicroStepsPerSecond(
            maxMilliMicroStepsPerSecond));
    serial.writeString(" rps ");
    serial.writeUnsignedFixed3(
        motor.milliDegreesPerSecondFromMilliMicroStepsPerSecond(
            maxMilliMicroStepsPerSecond));
    serial.writeLine(" deg/s");
}
