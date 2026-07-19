#include "AvrExternalPowerSupplyDetector.h"
#include "AvrGpioLed.h"
#include "AvrStepDirectionDriver.h"
#include "AvrSystemClock.h"
#include "AvrUartSerial.h"

#include "hal/ExternalPowerSupplyDetector.h"
#include "hal/GpioLed.h"
#include "hal/SystemClock.h"
#include "hal/UartSerial.h"

#include "motion/stepperMotor/StepperMotor.h"
#include "motion/stepperMotor/StepperMotorController.h"

struct StepperMotorProgram {
    StepperMotor motor;
    uint16_t stepPulseMicroseconds;
    uint16_t directionSetupMicroseconds;
    uint16_t travelRotations;
    uint16_t accelerationMilliseconds;
    uint16_t cruiseMilliseconds;
    uint16_t decelerationMilliseconds;
};

static const StepperMotorProgram STEPPER_MOTOR_PROGRAMS[] = {
    {
        StepperMotor(
            BIPOLAR_STEPPER_MOTOR,
            "Artillery D42HSA5401-23B",
            200U,
            BIPOLAR_STEPPER_MOTOR_MICROSTEP_16,
            STEPPER_MOTOR_STEP_PIN,
            STEPPER_MOTOR_DIRECTION_PIN),
        STEPPER_MOTOR_STEP_PULSE_MICROSECONDS,
        STEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS,
        STEPPER_MOTOR_TRAVEL_ROTATIONS,
        STEPPER_MOTOR_ACCELERATION_MILLISECONDS,
        STEPPER_MOTOR_CRUISE_MILLISECONDS,
        STEPPER_MOTOR_DECELERATION_MILLISECONDS
    }
};

static const uint8_t STEPPER_MOTOR_COUNT = static_cast<uint8_t>(
    sizeof(STEPPER_MOTOR_PROGRAMS) / sizeof(STEPPER_MOTOR_PROGRAMS[0]));

static_assert(
    sizeof(STEPPER_MOTOR_PROGRAMS) / sizeof(STEPPER_MOTOR_PROGRAMS[0]) <= 255U,
    "The motor registry index uses uint8_t");

static bool
hasValidAndUniquePins(uint8_t motorIndex)
{
    const StepperMotor& motor = STEPPER_MOTOR_PROGRAMS[motorIndex].motor;
    if (motor.stepPin < 2U || motor.directionPin < 2U ||
        motor.stepPin == motor.directionPin) {
        return false;
    }

    for (uint8_t i = 0U; i < motorIndex; ++i) {
        const StepperMotor& previousMotor = STEPPER_MOTOR_PROGRAMS[i].motor;
        if (motor.stepPin == previousMotor.stepPin ||
            motor.stepPin == previousMotor.directionPin ||
            motor.directionPin == previousMotor.stepPin ||
            motor.directionPin == previousMotor.directionPin) {
            return false;
        }
    }
    return true;
}

static void
printStepperMotorConfiguration(
    UartSerial& serial,
    const StepperMotorProgram& program,
    uint8_t motorIndex,
    uint32_t maxMilliMicroStepsPerSecond,
    bool motorReady)
{
    const StepperMotor& motor = program.motor;
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
    serial.writeUnsigned(program.travelRotations);
    serial.writeString(" rotations/");
    serial.writeUnsignedFixed3(
        static_cast<uint32_t>(program.accelerationMilliseconds) +
        program.cruiseMilliseconds + program.decelerationMilliseconds);
    serial.writeString("s, backward ");
    serial.writeUnsigned(program.travelRotations);
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

static void
printStepperMotorPosition(
    UartSerial& serial,
    const StepperMotor& motor,
    int32_t microStepPosition)
{
    const uint32_t microStepsPerRotation = motor.microStepsPerRotation();
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
        motor.centiDegreesFromMicroStepPosition(microStepPosition));
    serial.writeString(" degrees");
}

static void
printStepperMotorTelemetry(
    UartSerial& serial,
    const StepperMotor& motor,
    const StepperMotorController& controller,
    uint8_t motorIndex)
{
    const uint32_t speedMilliMicroStepsPerSecond =
        controller.speedMilliStepsPerSecond();

    serial.writeString(" Motor ");
    serial.writeUnsigned(motorIndex);
    serial.writeString(" Dir: ");
    serial.writeString(controller.directionForward() ? "F" : "R");
    serial.writeString(" Position: ");
    printStepperMotorPosition(serial, motor, controller.position());
    serial.writeString(" Speed: ");
    serial.writeUnsignedFixed3(
        motor.milliRotationsPerSecondFromMilliMicroStepsPerSecond(
            speedMilliMicroStepsPerSecond));
    serial.writeString(" rps ");
    serial.writeUnsignedFixed3(
        motor.milliDegreesPerSecondFromMilliMicroStepsPerSecond(
            speedMilliMicroStepsPerSecond));
    serial.writeString(" deg/s");
}

int
main()
{
    AvrSystemClock avrSystemClock;
    SystemClock& systemClock = avrSystemClock;
    systemClock.initialize();

    AvrExternalPowerSupplyDetector avrExternalPowerSupplyDetector;
    ExternalPowerSupplyDetector& externalPowerSupplyDetector =
        avrExternalPowerSupplyDetector;
    externalPowerSupplyDetector.initialize();

    AvrGpioLed avrStatusLed;
    GpioLed& statusLed = avrStatusLed;
    statusLed.initialize();

    AvrUartSerial avrSerial;
    UartSerial& serial = avrSerial;
    serial.initialize(ARDUINO_SERIAL_BAUD);
    serial.writeString("motionControl boot build=");
    serial.writeLine(MOTION_CONTROL_BUILD_TIMESTAMP);

    AvrStepDirectionDriver stepperMotorDrivers[STEPPER_MOTOR_COUNT];
    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT];
    uint32_t maxMilliMicroStepsPerSecond[STEPPER_MOTOR_COUNT];
    bool stepperMotorReady[STEPPER_MOTOR_COUNT];

    for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
        const StepperMotorProgram& program = STEPPER_MOTOR_PROGRAMS[i];
        maxMilliMicroStepsPerSecond[i] =
            program.motor.milliMicroStepsPerSecondForTrapezoidRotations(
                program.travelRotations,
                program.accelerationMilliseconds,
                program.cruiseMilliseconds,
                program.decelerationMilliseconds);

        stepperMotorDrivers[i].configure(
            program.motor.stepPin,
            program.motor.directionPin,
            program.stepPulseMicroseconds,
            program.directionSetupMicroseconds);
        stepperMotorReady[i] = hasValidAndUniquePins(i) &&
            stepperMotorControllers[i].initialize(stepperMotorDrivers[i]);
        stepperMotorControllers[i].configureRepeatingTrapezoid(
            maxMilliMicroStepsPerSecond[i],
            program.accelerationMilliseconds,
            program.cruiseMilliseconds,
            program.decelerationMilliseconds);

        printStepperMotorConfiguration(
            serial,
            program,
            i,
            maxMilliMicroStepsPerSecond[i],
            stepperMotorReady[i]);
    }

    serial.writeLine(
        "PSU filter: sample=10ms IIR=1/8 enable>=11.600V/100ms "
        "disable<11.200V/100ms");

    uint32_t lastTelemetryPrint = systemClock.millis();
    bool previousExternalPowerSupplyAvailable = false;

    //noinspection CppDFAEndlessLoop
    // NOLINTNEXTLINE(bugprone-infinite-loop)
    for (;;) {
        const uint32_t now = systemClock.millis();
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
                externalPowerSupplyAvailable && stepperMotorReady[i]);
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
                    STEPPER_MOTOR_PROGRAMS[i].motor,
                    stepperMotorControllers[i],
                    i);
            }
            serial.writeLine("");
        }
    }
}
