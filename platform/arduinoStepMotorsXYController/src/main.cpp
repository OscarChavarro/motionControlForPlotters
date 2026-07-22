#include "AvrExternalPowerSupplyDetector.h"
#include "AvrGpioLed.h"
#include "AvrStepDirectionDriver.h"
#include "AvrSystemClock.h"
#include "AvrUartSerial.h"
#include "StepperMotorProgram.h"

#include "hal/ExternalPowerSupplyDetector.h"
#include "hal/GpioLed.h"
#include "hal/SystemClock.h"
#include "hal/UartSerial.h"

#include "motion/stepperMotor/StepperMotor.h"
#include "motion/stepperMotor/StepperMotorController.h"

static const uint8_t COMMAND_BUFFER_SIZE = 32U;

static const StepperMotorProgram STEPPER_MOTOR_PROGRAMS[] = {
    StepperMotorProgram(
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
        STEPPER_MOTOR_DECELERATION_MILLISECONDS)
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
printAvailableCommands(UartSerial& serial)
{
    serial.writeLine("Available commands:");
    serial.writeLine("  .  Print one telemetry line.");
    serial.writeLine("  help  Show the list of available commands.");
    serial.writeLine("  hardware  List hardware elements by pin.");
    serial.writeLine("  console enable  Enable periodic console output.");
    serial.writeLine("  console disable Disable periodic console output.");
}

static void
printHardwareConfiguration(UartSerial& serial)
{
    serial.writeLine("Hardware by pin:");

    for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
        const StepperMotor& motor = STEPPER_MOTOR_PROGRAMS[i].motor;

        serial.writeString("  D");
        serial.writeUnsigned(motor.directionPin);
        serial.writeString(": stepper motor ");
        serial.writeUnsigned(i);
        serial.writeString(" direction, ");
        serial.writeString(motor.referenceName);
        serial.writeLine("");

        serial.writeString("  D");
        serial.writeUnsigned(motor.stepPin);
        serial.writeString(": stepper motor ");
        serial.writeUnsigned(i);
        serial.writeString(" step, ");
        serial.writeString(motor.referenceName);
        serial.writeLine("");
    }

    serial.writeLine("  A0: external power supply detector input");
}

static bool
commandEquals(const char* command, const char* expected)
{
    while (*command != '\0' && *expected != '\0') {
        if (*command != *expected) {
            return false;
        }
        ++command;
        ++expected;
    }
    return *command == '\0' && *expected == '\0';
}

static void
handleCommand(
    UartSerial& serial,
    const char* command,
    bool& consoleEnabled,
    bool& singleTelemetryRequested)
{
    if (command[0] == '\0') {
        return;
    }

    if (commandEquals(command, ".")) {
        singleTelemetryRequested = true;
        return;
    }

    if (commandEquals(command, "help")) {
        printAvailableCommands(serial);
        return;
    }

    if (commandEquals(command, "hardware")) {
        printHardwareConfiguration(serial);
        return;
    }

    if (commandEquals(command, "console enable")) {
        consoleEnabled = true;
        serial.writeLine("Console output enabled.");
        return;
    }

    if (commandEquals(command, "console disable")) {
        consoleEnabled = false;
        serial.writeLine("Console output disabled.");
        return;
    }

    serial.writeString("Unknown command: ");
    serial.writeLine(command);
}

static void
pollCommandInput(
    UartSerial& serial,
    char commandBuffer[COMMAND_BUFFER_SIZE],
    uint8_t& commandLength,
    bool& consoleEnabled,
    bool& singleTelemetryRequested)
{
    while (serial.isReadAvailable()) {
        const char received = serial.readChar();

        if (received == '\r' || received == '\n') {
            commandBuffer[commandLength] = '\0';
            handleCommand(
                serial,
                commandBuffer,
                consoleEnabled,
                singleTelemetryRequested);
            commandLength = 0U;
            continue;
        }

        if (received == '\b' || received == 127) {
            if (commandLength > 0U) {
                --commandLength;
            }
            continue;
        }

        if (received < 32 || received > 126) {
            continue;
        }

        if (commandLength + 1U < COMMAND_BUFFER_SIZE) {
            commandBuffer[commandLength++] = received;
        }
        else {
            serial.writeLine("Command too long.");
            commandLength = 0U;
        }
    }
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

static void
initializeHardware(
    SystemClock& systemClock,
    ExternalPowerSupplyDetector& externalPowerSupplyDetector,
    GpioLed& statusLed,
    UartSerial& serial)
{
    systemClock.initialize();
    externalPowerSupplyDetector.initialize();
    statusLed.initialize();
    serial.initialize(ARDUINO_SERIAL_BAUD);
    serial.writeString("motionControl boot build=");
    serial.writeLine(MOTION_CONTROL_BUILD_TIMESTAMP);
    serial.writeLine(
        "PSU filter: sample=10ms IIR=1/8 enable>=11.600V/100ms "
        "disable<11.200V/100ms");
    printAvailableCommands(serial);
}

static void
initializeStepperMotorPrograms(
    UartSerial& serial,
    AvrStepDirectionDriver stepperMotorDrivers[STEPPER_MOTOR_COUNT],
    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT],
    uint32_t maxMilliMicroStepsPerSecond[STEPPER_MOTOR_COUNT],
    bool stepperMotorReady[STEPPER_MOTOR_COUNT],
    bool consoleEnabled)
{
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

        if (consoleEnabled) {
            program.printConfiguration(
                serial,
                i,
                maxMilliMicroStepsPerSecond[i],
                stepperMotorReady[i]);
        }
    }
}

static bool
updateExternalPowerSupplyStatus(
    UartSerial& serial,
    ExternalPowerSupplyDetector& externalPowerSupplyDetector,
    uint32_t now,
    bool previousExternalPowerSupplyAvailable,
    bool consoleEnabled)
{
    const bool newPowerSupplySample = externalPowerSupplyDetector.update(now);
    const bool externalPowerSupplyAvailable =
        externalPowerSupplyDetector.isExternalPowerSupplyAvailable();

    if (consoleEnabled &&
        newPowerSupplySample &&
        externalPowerSupplyAvailable != previousExternalPowerSupplyAvailable) {
        serial.writeString("EVENT PSU=");
        serial.writeString(externalPowerSupplyAvailable ? "READY" : "LOST");
        serial.writeString(" VMotor=");
        serial.writeVoltageMillivolts(
            externalPowerSupplyDetector.filteredExternalSupplyMillivolts());
        serial.writeLine("");
    }

    return externalPowerSupplyAvailable;
}

static void
updateStepperMotorControllers(
    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT],
    const bool stepperMotorReady[STEPPER_MOTOR_COUNT],
    uint32_t now,
    bool externalPowerSupplyAvailable)
{
    for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
        stepperMotorControllers[i].update(
            now,
            externalPowerSupplyAvailable && stepperMotorReady[i]);
    }
}

static void
printTelemetry(
    UartSerial& serial,
    ExternalPowerSupplyDetector& externalPowerSupplyDetector,
    const StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT])
{
    serial.writeString("VMotor: ");
    serial.writeVoltageMillivolts(
        externalPowerSupplyDetector.filteredExternalSupplyMillivolts());
    serial.writeString(" PSU: ");
    serial.writeString(
        externalPowerSupplyDetector.isExternalPowerSupplyAvailable() ?
            "OK" :
            "OFF");
    for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
        printStepperMotorTelemetry(
            serial,
            STEPPER_MOTOR_PROGRAMS[i].motor,
            stepperMotorControllers[i],
            i);
    }
    serial.writeLine("");
}

static void
mainLoopBody(
    SystemClock& systemClock,
    UartSerial& serial,
    ExternalPowerSupplyDetector& externalPowerSupplyDetector,
    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT],
    const bool stepperMotorReady[STEPPER_MOTOR_COUNT],
    uint32_t& lastTelemetryPrint,
    bool& previousExternalPowerSupplyAvailable,
    char commandBuffer[COMMAND_BUFFER_SIZE],
    uint8_t& commandLength,
    bool& consoleEnabled,
    bool& singleTelemetryRequested)
{
    const uint32_t now = systemClock.millis();
    pollCommandInput(
        serial,
        commandBuffer,
        commandLength,
        consoleEnabled,
        singleTelemetryRequested);
    const bool externalPowerSupplyAvailable =
        updateExternalPowerSupplyStatus(
            serial,
            externalPowerSupplyDetector,
            now,
            previousExternalPowerSupplyAvailable,
            consoleEnabled);
    previousExternalPowerSupplyAvailable = externalPowerSupplyAvailable;

    updateStepperMotorControllers(
        stepperMotorControllers,
        stepperMotorReady,
        now,
        externalPowerSupplyAvailable);

    if (singleTelemetryRequested) {
        printTelemetry(
            serial,
            externalPowerSupplyDetector,
            stepperMotorControllers);
        singleTelemetryRequested = false;
    }
    else if (consoleEnabled && (now - lastTelemetryPrint) >= 500UL) {
        lastTelemetryPrint = now;
        printTelemetry(
            serial,
            externalPowerSupplyDetector,
            stepperMotorControllers);
    }
}

int
main()
{
    AvrSystemClock avrSystemClock;
    SystemClock& systemClock = avrSystemClock;

    AvrExternalPowerSupplyDetector avrExternalPowerSupplyDetector;
    ExternalPowerSupplyDetector& externalPowerSupplyDetector =
        avrExternalPowerSupplyDetector;

    AvrGpioLed avrStatusLed;
    GpioLed& statusLed = avrStatusLed;

    AvrUartSerial avrSerial;
    UartSerial& serial = avrSerial;
    bool consoleEnabled = false;

    initializeHardware(
        systemClock,
        externalPowerSupplyDetector,
        statusLed,
        serial);

    AvrStepDirectionDriver stepperMotorDrivers[STEPPER_MOTOR_COUNT];
    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT];
    uint32_t maxMilliMicroStepsPerSecond[STEPPER_MOTOR_COUNT];
    bool stepperMotorReady[STEPPER_MOTOR_COUNT];
    initializeStepperMotorPrograms(
        serial,
        stepperMotorDrivers,
        stepperMotorControllers,
        maxMilliMicroStepsPerSecond,
        stepperMotorReady,
        consoleEnabled);

    uint32_t lastTelemetryPrint = systemClock.millis();
    bool previousExternalPowerSupplyAvailable = false;
    bool singleTelemetryRequested = false;
    char commandBuffer[COMMAND_BUFFER_SIZE] = {0};
    uint8_t commandLength = 0U;

    //noinspection CppDFAEndlessLoop
    // NOLINTNEXTLINE(bugprone-infinite-loop)
    for (;;) {
        mainLoopBody(
            systemClock,
            serial,
            externalPowerSupplyDetector,
            stepperMotorControllers,
            stepperMotorReady,
            lastTelemetryPrint,
            previousExternalPowerSupplyAvailable,
            commandBuffer,
            commandLength,
            consoleEnabled,
            singleTelemetryRequested);
    }
}
