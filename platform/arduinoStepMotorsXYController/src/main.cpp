#include "AvrExternalPowerSupplyDetector.h"
#include "AvrGpioLed.h"
#include "AvrMotorDriverEnable.h"
#include "AvrStepDirectionDriver.h"
#include "AvrSystemClock.h"
#include "AvrUartSerial.h"
#include "StepperMotorProgram.h"

#include "hal/ExternalPowerSupplyDetector.h"
#include "hal/GpioLed.h"
#include "hal/MotorDriverEnable.h"
#include "hal/SystemClock.h"
#include "hal/UartSerial.h"

#include "motion/stepperMotor/StepperMotor.h"
#include "motion/stepperMotor/StepperMotorController.h"

static const uint8_t COMMAND_BUFFER_SIZE = 32U;

static const StepperMotorProgram STEPPER_MOTOR_PROGRAMS[] = {
    StepperMotorProgram(
        StepperMotor(
            BIPOLAR_STEPPER_MOTOR,
            "D42HSC4409B-23B",
            200U,
            BIPOLAR_STEPPER_MOTOR_MICROSTEP_16,
            STEPPER_MOTOR_STEP_PIN,
            STEPPER_MOTOR_DIRECTION_PIN),
        STEPPER_MOTOR_STEP_PULSE_MICROSECONDS,
        STEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS,
        STEPPER_MOTOR_TRAVEL_ROTATIONS,
        STEPPER_MOTOR_ACCELERATION_MILLISECONDS,
        STEPPER_MOTOR_CRUISE_MILLISECONDS,
        STEPPER_MOTOR_DECELERATION_MILLISECONDS,
        "FS31W01 191202",
        StepperMotorProgram::NO_UART_PIN,
        StepperMotorProgram::NO_UART_PIN,
        false),
    StepperMotorProgram(
        StepperMotor(
            BIPOLAR_STEPPER_MOTOR,
            "D42HSC4409B-23B",
            STEPPER_MOTOR_Y_FULL_STEPS_PER_ROTATION,
            BIPOLAR_STEPPER_MOTOR_MICROSTEP_16,
            STEPPER_MOTOR_Y_STEP_PIN,
            STEPPER_MOTOR_Y_DIRECTION_PIN),
        STEPPER_MOTOR_STEP_PULSE_MICROSECONDS,
        STEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS,
        STEPPER_MOTOR_Y_TRAVEL_ROTATIONS,
        STEPPER_MOTOR_ACCELERATION_MILLISECONDS,
        STEPPER_MOTOR_CRUISE_MILLISECONDS,
        STEPPER_MOTOR_DECELERATION_MILLISECONDS,
        "FS31W01 191202",
        StepperMotorProgram::NO_UART_PIN,
        StepperMotorProgram::NO_UART_PIN,
        STEPPER_MOTOR_Y_DIRECTION_INVERTED != 0)
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
    serial.writeLine("  . [<id>]  Print telemetry, or one element with an id.");
    serial.writeLine("  help  Show the list of available commands.");
    serial.writeLine("  hardware  List hardware elements by pin, tagged [id].");
    serial.writeLine("  get <id>  Print status of one hardware element.");
    serial.writeLine("  console enable  Enable periodic console output.");
    serial.writeLine("  console disable Disable periodic console output.");
    serial.writeLine("  test <id> enable|disable  Toggle test movement for element <id>.");
    serial.writeLine("  motordriver <id> enable|disable  Toggle the shared motor driver enable line (PSU element <id>).");
    serial.writeLine("  steps <id> <n> <speed>  Move motor <id> by n steps at speed steps/s (blocking; test must be disabled).");
}

static void
printHardwareConfiguration(UartSerial& serial)
{
    serial.writeLine("Hardware by pin:");

    for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
        const StepperMotorProgram& program = STEPPER_MOTOR_PROGRAMS[i];
        const StepperMotor& motor = program.motor;

        serial.writeString("[");
        serial.writeUnsigned(i);
        serial.writeString("] D");
        serial.writeUnsigned(motor.directionPin);
        serial.writeString(", D");
        serial.writeUnsigned(motor.stepPin);
        serial.writeString(": stepper motor ");
        serial.writeUnsigned(i);
        serial.writeString(" (direction, step), ");
        serial.writeString(motor.referenceName);
        serial.writeString(" driven by ");
        serial.writeString(program.driverDescription);

        if (program.uartReceivePin != StepperMotorProgram::NO_UART_PIN) {
            serial.writeString(" (UART: RX=D");
            serial.writeUnsigned(program.uartReceivePin);
            serial.writeString(", TX=D");
            serial.writeUnsigned(program.uartTransmitPin);
            serial.writeString(")");
        }
        serial.writeLine("");
    }

    serial.writeString("[");
    serial.writeUnsigned(STEPPER_MOTOR_COUNT);
    serial.writeString("] A5, D");
    serial.writeUnsigned(MOTOR_DRIVER_ENABLE_PIN);
    serial.writeString(
        ": external power supply detector input, motor driver enable");
    serial.writeLine("");
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

static bool
commandStartsWith(const char* command, const char* prefix, const char*& remainder)
{
    while (*prefix != '\0') {
        if (*command != *prefix) {
            return false;
        }
        ++command;
        ++prefix;
    }
    remainder = command;
    return true;
}

static bool
parseUnsignedId(const char* text, uint16_t& value)
{
    if (*text < '0' || *text > '9') {
        return false;
    }

    uint16_t result = 0U;
    while (*text >= '0' && *text <= '9') {
        result = static_cast<uint16_t>(
            result * 10U + static_cast<uint16_t>(*text - '0'));
        ++text;
    }
    value = result;
    return true;
}

// Parses a leading signed integer and reports where it ends, so callers
// can chain several numbers separated by spaces (id, steps, speed)
// without requiring each one to consume the rest of the command string.
static bool
parseSignedInt32(const char* text, int32_t& value, const char*& afterValue)
{
    const char* cursor = text;
    bool negative = false;
    if (*cursor == '-') {
        negative = true;
        ++cursor;
    }

    if (*cursor < '0' || *cursor > '9') {
        return false;
    }

    int32_t result = 0L;
    while (*cursor >= '0' && *cursor <= '9') {
        result = result * 10L + (*cursor - '0');
        ++cursor;
    }

    value = negative ? -result : result;
    afterValue = cursor;
    return true;
}

// Every hardware element (stepper motors, then the power supply detector)
// gets a stable numeric id, assigned in the same order printed by
// printHardwareConfiguration: motors are 0..STEPPER_MOTOR_COUNT-1, and the
// power supply detector is STEPPER_MOTOR_COUNT.
static void
printElementStatus(
    UartSerial& serial,
    uint16_t id,
    ExternalPowerSupplyDetector& externalPowerSupplyDetector,
    const StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT],
    const bool stepperMotorTestEnabled[STEPPER_MOTOR_COUNT],
    bool motorDriverEnabledRequested)
{
    if (id < STEPPER_MOTOR_COUNT) {
        const StepperMotorController& controller = stepperMotorControllers[id];
        serial.writeString("MOTOR,");
        serial.writeString(controller.directionForward() ? "F" : "R");
        serial.writeString(",");
        serial.writeUnsigned(controller.speedMilliStepsPerSecond());
        serial.writeString(",");
        serial.writeSigned(controller.position());
        serial.writeString(",");
        serial.writeString(stepperMotorTestEnabled[id] ? "1" : "0");
        serial.writeLine("");
        return;
    }

    if (id == STEPPER_MOTOR_COUNT) {
        serial.writeString("PSU,");
        serial.writeString(
            externalPowerSupplyDetector.isExternalPowerSupplyAvailable() ?
                "ON" :
                "OFF");
        serial.writeString(",");
        serial.writeUnsigned(
                externalPowerSupplyDetector.filteredExternalSupplyMilliVolts());
        serial.writeString(",");
        serial.writeString(motorDriverEnabledRequested ? "1" : "0");
        serial.writeLine("");
        return;
    }

    serial.writeLine("Unknown element id.");
}

static void
handleCommand(
    UartSerial& serial,
    const char* command,
    ExternalPowerSupplyDetector& externalPowerSupplyDetector,
    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT],
    const bool stepperMotorReady[STEPPER_MOTOR_COUNT],
    bool stepperMotorTestEnabled[STEPPER_MOTOR_COUNT],
    bool& motorDriverEnabledRequested,
    bool& consoleEnabled,
    bool& singleTelemetryRequested,
    uint16_t& singleTelemetryId)
{
    if (command[0] == '\0') {
        return;
    }

    if (commandEquals(command, ".")) {
        singleTelemetryRequested = true;
        singleTelemetryId = STEPPER_MOTOR_COUNT + 1U;
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

    const char* idText = nullptr;
    if (commandStartsWith(command, ". ", idText)) {
        uint16_t id = 0U;
        if (parseUnsignedId(idText, id)) {
            singleTelemetryRequested = true;
            singleTelemetryId = id;
        }
        else {
            serial.writeLine("Usage: . [<id>]");
        }
        return;
    }

    if (commandStartsWith(command, "get ", idText)) {
        uint16_t id = 0U;
        if (parseUnsignedId(idText, id)) {
            printElementStatus(
                serial,
                id,
                externalPowerSupplyDetector,
                stepperMotorControllers,
                stepperMotorTestEnabled,
                motorDriverEnabledRequested);
        }
        else {
            serial.writeLine("Usage: get <id>");
        }
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

    if (commandStartsWith(command, "test ", idText)) {
        uint16_t id = 0U;
        const char* stateText = idText;
        if (parseUnsignedId(idText, id) && id < STEPPER_MOTOR_COUNT) {
            while (*stateText >= '0' && *stateText <= '9') {
                ++stateText;
            }
            if (*stateText == ' ') {
                ++stateText;
            }
            if (commandEquals(stateText, "enable")) {
                if (!motorDriverEnabledRequested ||
                    !externalPowerSupplyDetector
                         .isExternalPowerSupplyAvailable()) {
                    serial.writeLine("Error: motor driver disabled.");
                    return;
                }
                stepperMotorTestEnabled[id] = true;
                serial.writeString("Test movement enabled for element ");
                serial.writeUnsigned(id);
                serial.writeLine("");
                return;
            }
            if (commandEquals(stateText, "disable")) {
                stepperMotorTestEnabled[id] = false;
                serial.writeString("Test movement disabled for element ");
                serial.writeUnsigned(id);
                serial.writeLine("");
                return;
            }
        }
        serial.writeLine("Usage: test <id> enable|disable");
        return;
    }

    if (commandStartsWith(command, "motordriver ", idText)) {
        uint16_t id = 0U;
        const char* stateText = idText;
        if (parseUnsignedId(idText, id) && id == STEPPER_MOTOR_COUNT) {
            while (*stateText >= '0' && *stateText <= '9') {
                ++stateText;
            }
            if (*stateText == ' ') {
                ++stateText;
            }
            if (commandEquals(stateText, "enable")) {
                motorDriverEnabledRequested = true;
                serial.writeLine("Motor driver enabled.");
                return;
            }
            if (commandEquals(stateText, "disable")) {
                motorDriverEnabledRequested = false;
                serial.writeLine("Motor driver disabled.");
                return;
            }
        }
        serial.writeLine("Usage: motordriver <id> enable|disable");
        return;
    }

    if (commandStartsWith(command, "steps ", idText)) {
        uint16_t id = 0U;
        const char* cursor = idText;
        if (parseUnsignedId(idText, id) && id < STEPPER_MOTOR_COUNT) {
            while (*cursor >= '0' && *cursor <= '9') {
                ++cursor;
            }
            if (*cursor == ' ') {
                ++cursor;
            }

            int32_t steps = 0L;
            const char* afterSteps = nullptr;
            if (parseSignedInt32(cursor, steps, afterSteps) && steps != 0L &&
                *afterSteps == ' ') {
                int32_t speed = 0L;
                const char* afterSpeed = nullptr;
                if (parseSignedInt32(afterSteps + 1, speed, afterSpeed) &&
                    speed > 0L && *afterSpeed == '\0') {
                    if (stepperMotorTestEnabled[id]) {
                        serial.writeLine(
                            "Error: disable test movement before using steps.");
                    }
                    else if (!stepperMotorReady[id]) {
                        serial.writeLine("Error: element not ready.");
                    }
                    else if (!motorDriverEnabledRequested) {
                        serial.writeLine("Error: motor driver disabled.");
                    }
                    else if (!externalPowerSupplyDetector
                                  .isExternalPowerSupplyAvailable()) {
                        serial.writeLine(
                            "Error: external power supply not available.");
                    }
                    else {
                        stepperMotorControllers[id].moveBlockingSteps(
                            steps, static_cast<uint32_t>(speed));
                        serial.writeString("Moved element ");
                        serial.writeUnsigned(id);
                        serial.writeString(" by ");
                        serial.writeSigned(steps);
                        serial.writeString(" steps at ");
                        serial.writeUnsigned(static_cast<uint32_t>(speed));
                        serial.writeLine(" steps/s.");
                    }
                    return;
                }
            }
        }
        serial.writeLine("Usage: steps <id> <n> <speed> (n != 0, speed > 0)");
        return;
    }

    serial.writeString("Unknown command: ");
    serial.writeLine(command);
}

static void
pollCommandInput(
    UartSerial& serial,
    ExternalPowerSupplyDetector& externalPowerSupplyDetector,
    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT],
    const bool stepperMotorReady[STEPPER_MOTOR_COUNT],
    bool stepperMotorTestEnabled[STEPPER_MOTOR_COUNT],
    bool& motorDriverEnabledRequested,
    char commandBuffer[COMMAND_BUFFER_SIZE],
    uint8_t& commandLength,
    bool& consoleEnabled,
    bool& singleTelemetryRequested,
    uint16_t& singleTelemetryId)
{
    while (serial.isReadAvailable()) {
        const char received = serial.readChar();

        if (received == '\r' || received == '\n') {
            commandBuffer[commandLength] = '\0';
            handleCommand(
                serial,
                commandBuffer,
                externalPowerSupplyDetector,
                stepperMotorControllers,
                stepperMotorReady,
                stepperMotorTestEnabled,
                motorDriverEnabledRequested,
                consoleEnabled,
                singleTelemetryRequested,
                singleTelemetryId);
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
    MotorDriverEnable& motorDriverEnable,
    UartSerial& serial)
{
    systemClock.initialize();
    externalPowerSupplyDetector.initialize();
    statusLed.initialize();
    motorDriverEnable.initialize();
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
            program.directionSetupMicroseconds,
            program.directionInverted);
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
    bool previousExternalPowerSupplyAvailable)
{
    const bool newPowerSupplySample = externalPowerSupplyDetector.update(now);
    const bool externalPowerSupplyAvailable =
        externalPowerSupplyDetector.isExternalPowerSupplyAvailable();

    // Unlike periodic telemetry, this is a low-frequency state-change
    // event, so it is always reported regardless of "console enable" -
    // clients that only poll individual elements still need to learn
    // about a PSU disconnect without opting into the 500ms telemetry
    // stream.
    if (newPowerSupplySample &&
        externalPowerSupplyAvailable != previousExternalPowerSupplyAvailable) {
        serial.writeString("EVENT PSU=");
        serial.writeString(externalPowerSupplyAvailable ? "READY" : "LOST");
        serial.writeString(" VMotor=");
        serial.writeVoltageMillivolts(
                externalPowerSupplyDetector.filteredExternalSupplyMilliVolts());
        serial.writeLine("");
    }

    return externalPowerSupplyAvailable;
}

static void
updateStepperMotorControllers(
    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT],
    const bool stepperMotorReady[STEPPER_MOTOR_COUNT],
    const bool stepperMotorTestEnabled[STEPPER_MOTOR_COUNT],
    uint32_t now,
    bool motorsActuallyEnabled)
{
    for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
        stepperMotorControllers[i].update(
            now,
            stepperMotorTestEnabled[i] &&
                motorsActuallyEnabled && stepperMotorReady[i]);
    }
}

static void
printTelemetry(
    UartSerial& serial,
    ExternalPowerSupplyDetector& externalPowerSupplyDetector,
    const StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT],
    const bool stepperMotorTestEnabled[STEPPER_MOTOR_COUNT],
    bool motorDriverEnabledRequested,
    uint16_t requestedId)
{
    if (requestedId <= STEPPER_MOTOR_COUNT) {
        printElementStatus(
            serial,
            requestedId,
            externalPowerSupplyDetector,
            stepperMotorControllers,
            stepperMotorTestEnabled,
            motorDriverEnabledRequested);
        return;
    }

    serial.writeString("VMotor: ");
    serial.writeVoltageMillivolts(
            externalPowerSupplyDetector.filteredExternalSupplyMilliVolts());
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
    MotorDriverEnable& motorDriverEnable,
    StepperMotorController stepperMotorControllers[STEPPER_MOTOR_COUNT],
    const bool stepperMotorReady[STEPPER_MOTOR_COUNT],
    bool stepperMotorTestEnabled[STEPPER_MOTOR_COUNT],
    bool& motorDriverEnabledRequested,
    uint32_t& lastTelemetryPrint,
    bool& previousExternalPowerSupplyAvailable,
    char commandBuffer[COMMAND_BUFFER_SIZE],
    uint8_t& commandLength,
    bool& consoleEnabled,
    bool& singleTelemetryRequested,
    uint16_t& singleTelemetryId)
{
    const uint32_t now = systemClock.millis();
    pollCommandInput(
        serial,
        externalPowerSupplyDetector,
        stepperMotorControllers,
        stepperMotorReady,
        stepperMotorTestEnabled,
        motorDriverEnabledRequested,
        commandBuffer,
        commandLength,
        consoleEnabled,
        singleTelemetryRequested,
        singleTelemetryId);
    const bool externalPowerSupplyAvailable =
        updateExternalPowerSupplyStatus(
            serial,
            externalPowerSupplyDetector,
            now,
            previousExternalPowerSupplyAvailable);
    previousExternalPowerSupplyAvailable = externalPowerSupplyAvailable;

    // The driver enable line only ever goes active when both the user
    // wants motors on (the panic-button flag) and a PSU is actually
    // present: no PSU always means DISABLED, regardless of the flag.
    const bool motorsActuallyEnabled =
        motorDriverEnabledRequested && externalPowerSupplyAvailable;
    motorDriverEnable.setEnabled(motorsActuallyEnabled);

    updateStepperMotorControllers(
        stepperMotorControllers,
        stepperMotorReady,
        stepperMotorTestEnabled,
        now,
        motorsActuallyEnabled);

    if (singleTelemetryRequested) {
        printTelemetry(
            serial,
            externalPowerSupplyDetector,
            stepperMotorControllers,
            stepperMotorTestEnabled,
            motorDriverEnabledRequested,
            singleTelemetryId);
        singleTelemetryRequested = false;
    }
    else if (consoleEnabled && (now - lastTelemetryPrint) >= 500UL) {
        lastTelemetryPrint = now;
        printTelemetry(
            serial,
            externalPowerSupplyDetector,
            stepperMotorControllers,
            stepperMotorTestEnabled,
            motorDriverEnabledRequested,
            STEPPER_MOTOR_COUNT + 1U);
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

    AvrMotorDriverEnable avrMotorDriverEnable;
    MotorDriverEnable& motorDriverEnable = avrMotorDriverEnable;

    AvrUartSerial avrSerial;
    UartSerial& serial = avrSerial;
    bool consoleEnabled = false;

    initializeHardware(
        systemClock,
        externalPowerSupplyDetector,
        statusLed,
        motorDriverEnable,
        serial);

    // Defaults to true: motors run whenever a PSU is present, until the
    // user (or the app, e.g. a panic button) explicitly disables them via
    // "motordriver <id> disable".
    bool motorDriverEnabledRequested = true;

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

    // Per-motor "test" toggle, independent for each motor instance so
    // several motors can each be started/stopped on their own.
    bool stepperMotorTestEnabled[STEPPER_MOTOR_COUNT];
    for (uint8_t i = 0U; i < STEPPER_MOTOR_COUNT; ++i) {
        stepperMotorTestEnabled[i] = true;
    }

    uint32_t lastTelemetryPrint = systemClock.millis();
    bool previousExternalPowerSupplyAvailable = false;
    bool singleTelemetryRequested = false;
    uint16_t singleTelemetryId = STEPPER_MOTOR_COUNT + 1U;
    char commandBuffer[COMMAND_BUFFER_SIZE] = {0};
    uint8_t commandLength = 0U;

    //noinspection CppDFAEndlessLoop
    // NOLINTNEXTLINE(bugprone-infinite-loop)
    for (;;) {
        mainLoopBody(
            systemClock,
            serial,
            externalPowerSupplyDetector,
            motorDriverEnable,
            stepperMotorControllers,
            stepperMotorReady,
            stepperMotorTestEnabled,
            motorDriverEnabledRequested,
            lastTelemetryPrint,
            previousExternalPowerSupplyAvailable,
            commandBuffer,
            commandLength,
            consoleEnabled,
            singleTelemetryRequested,
            singleTelemetryId);
    }
}
