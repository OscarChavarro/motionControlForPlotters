#include "ExternalPowerSupplyDetector.h"
#include "StepperMotorController.h"
#include "SystemClock.h"
#include "UartSerial.h"

#include "motion/stepperMotor/StepperMotor.h"

static const uint8_t STEPPER_MOTOR_COUNT = 1U;

static void
printStepperMotorConfiguration(
    UartSerial& serial,
    const StepperMotor& stepperMotor,
    uint8_t motorIndex,
    uint16_t travelRotations,
    uint32_t maxMilliMicroStepsPerSecond)
{
    serial.writeString("Stepper motor ");
    serial.writeUnsigned(motorIndex);
    serial.writeString(": referenceName=");
    serial.writeString(stepperMotor.referenceName);
    serial.writeString(" type=");
    serial.writeString(stepperMotor.typeName());
    serial.writeString(" fullSteps/rotation=");
    serial.writeUnsigned(stepperMotor.fullStepsPerRotation);
    serial.writeString(" microsteps=");
    serial.writeUnsigned(stepperMotor.microStepsPerFullStep());
    serial.writeString(" microsteps/rotation=");
    serial.writeUnsigned(stepperMotor.microStepsPerRotation());
    serial.writeString(" stepPin=");
    serial.writeUnsigned(stepperMotor.stepPin);
    serial.writeString(" directionPin=");
    serial.writeUnsigned(stepperMotor.directionPin);
    serial.writeLine("");

    serial.writeString("Stepper motor ");
    serial.writeUnsigned(motorIndex);
    serial.writeString(" movement=forward ");
    serial.writeUnsigned(travelRotations);
    serial.writeString(" rotations/10s, backward ");
    serial.writeUnsigned(travelRotations);
    serial.writeString(" rotations/10s maxSpeed=");
    serial.writeUnsignedFixed3(
        stepperMotor
            .milliRotationsPerSecondFromMilliMicroStepsPerSecond(
                maxMilliMicroStepsPerSecond));
    serial.writeString(" rps ");
    serial.writeUnsignedFixed3(
        stepperMotor
            .milliDegreesPerSecondFromMilliMicroStepsPerSecond(
                maxMilliMicroStepsPerSecond));
    serial.writeLine(" deg/s");
}

static void
printStepperMotorPosition(
    UartSerial& serial,
    const StepperMotor& stepperMotor,
    int32_t microStepPosition)
{
    const uint32_t microStepsPerRotation =
        stepperMotor.microStepsPerRotation();
    uint32_t absolutePosition = 0UL;

    if (microStepPosition < 0L) {
        absolutePosition =
            static_cast<uint32_t>(-(microStepPosition + 1L)) + 1UL;
    }
    else {
        absolutePosition = static_cast<uint32_t>(microStepPosition);
    }

    const uint32_t rotationIndex =
        (microStepsPerRotation == 0UL || absolutePosition == 0UL) ?
            0UL :
            (((absolutePosition - 1UL) / microStepsPerRotation) + 1UL);
    if (rotationIndex > 1UL) {
        serial.writeString("rotation ");
        serial.writeUnsigned(rotationIndex);
        serial.writeString(", ");
    }

    serial.writeUnsignedFixed2(
        stepperMotor.centiDegreesFromMicroStepPosition(microStepPosition));
    serial.writeString(" degrees");
}

static void
printStepperMotorTelemetry(
    UartSerial& serial,
    const StepperMotor& stepperMotor,
    const StepperMotorController& stepperMotorController,
    uint8_t motorIndex)
{
    const uint32_t speedMilliMicroStepsPerSecond =
        stepperMotorController.speedMilliStepsPerSecond();

    serial.writeString(" Motor ");
    serial.writeUnsigned(motorIndex);
    serial.writeString(" Dir: ");
    serial.writeString(stepperMotorController.directionForward() ? "F" : "R");
    serial.writeString(" Position: ");
    printStepperMotorPosition(
        serial,
        stepperMotor,
        stepperMotorController.position());
    serial.writeString(" Speed: ");
    serial.writeUnsignedFixed3(
        stepperMotor.milliRotationsPerSecondFromMilliMicroStepsPerSecond(
            speedMilliMicroStepsPerSecond));
    serial.writeString(" rps ");
    serial.writeUnsignedFixed3(
        stepperMotor.milliDegreesPerSecondFromMilliMicroStepsPerSecond(
            speedMilliMicroStepsPerSecond));
    serial.writeString(" deg/s");
}

int
main()
{
    SystemClock::initialize();

    StepperMotor stepperMotors[STEPPER_MOTOR_COUNT] = {
        StepperMotor(
            BIPOLAR_STEPPER_MOTOR,
            "Artillery D42HSA5401-23B",
            200U,
            BIPOLAR_STEPPER_MOTOR_MICROSTEP_16,
            STEPPER_MOTOR_STEP_PIN,
            STEPPER_MOTOR_DIRECTION_PIN)
    };

    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT] = {
        StepperMotorController(
            stepperMotors[0].stepPin,
            stepperMotors[0].directionPin,
            STEPPER_MOTOR_STEP_PULSE_MICROSECONDS,
            STEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS)
    };

    uint32_t maxMilliMicroStepsPerSecond[STEPPER_MOTOR_COUNT];
    for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
        maxMilliMicroStepsPerSecond[i] =
            stepperMotors[i]
                .milliMicroStepsPerSecondForTrapezoidRotations(
                    STEPPER_MOTOR_TRAVEL_ROTATIONS,
                    STEPPER_MOTOR_ACCELERATION_MILLISECONDS,
                    STEPPER_MOTOR_CRUISE_MILLISECONDS,
                    STEPPER_MOTOR_DECELERATION_MILLISECONDS);
        stepperMotorControllers[i].initialize();
        stepperMotorControllers[i].configureRepeatingTrapezoid(
            maxMilliMicroStepsPerSecond[i],
            STEPPER_MOTOR_ACCELERATION_MILLISECONDS,
            STEPPER_MOTOR_CRUISE_MILLISECONDS,
            STEPPER_MOTOR_DECELERATION_MILLISECONDS);
    }

    ExternalPowerSupplyDetector externalPowerSupplyDetector;
    externalPowerSupplyDetector.initialize();

    UartSerial serial;
    serial.initialize(ARDUINO_SERIAL_BAUD);
    serial.writeString("motionControl boot build=");
    serial.writeLine(MOTION_CONTROL_BUILD_TIMESTAMP);
    for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
        printStepperMotorConfiguration(
            serial,
            stepperMotors[i],
            i,
            STEPPER_MOTOR_TRAVEL_ROTATIONS,
            maxMilliMicroStepsPerSecond[i]);
    }
    serial.writeLine(
        "PSU filter: sample=10ms IIR=1/8 enable>=11.600V/100ms "
        "disable<11.200V/100ms");

    uint32_t lastTelemetryPrint = SystemClock::millis();
    bool previousExternalPowerSupplyAvailable = false;

    //noinspection CppDFAEndlessLoop
    // NOLINTNEXTLINE(bugprone-infinite-loop)
    for (;;) {
        const uint32_t now = SystemClock::millis();
        const bool newPowerSupplySample =
            externalPowerSupplyDetector.update(now);
        const bool externalPowerSupplyAvailable =
            externalPowerSupplyDetector.isExternalPowerSupplyAvailable();

        if (newPowerSupplySample &&
            externalPowerSupplyAvailable !=
                previousExternalPowerSupplyAvailable) {
            serial.writeString("EVENT PSU=");
            serial.writeString(
                externalPowerSupplyAvailable ? "READY" : "LOST");
            serial.writeString(" VIN=");
            serial.writeVoltageMillivolts(
                externalPowerSupplyDetector.filteredExternalSupplyMillivolts());
            serial.writeLine("");
        }
        previousExternalPowerSupplyAvailable =
            externalPowerSupplyAvailable;

        for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
            stepperMotorControllers[i].update(
                now,
                externalPowerSupplyAvailable);
        }

        if ((now - lastTelemetryPrint) >= 500UL) {
            lastTelemetryPrint = now;
            serial.writeString("VIN: ");
            serial.writeVoltageMillivolts(
                externalPowerSupplyDetector.filteredExternalSupplyMillivolts());
            serial.writeString(" PSU: ");
            serial.writeString(externalPowerSupplyAvailable ? "OK" : "OFF");
            for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
                printStepperMotorTelemetry(
                    serial,
                    stepperMotors[i],
                    stepperMotorControllers[i],
                    i);
            }
            serial.writeLine("");
        }
    }
}
