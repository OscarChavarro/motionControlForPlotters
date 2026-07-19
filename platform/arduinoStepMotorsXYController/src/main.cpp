#include "ExternalPowerSupplyDetector.h"
#include "StepperMotorController.h"
#include "SystemClock.h"
#include "UartSerial.h"

int
main()
{
    SystemClock::initialize();

    StepperMotorController stepperMotorController(
        STEPPER_MOTOR_STEP_PIN,
        STEPPER_MOTOR_DIRECTION_PIN,
        STEPPER_MOTOR_STEP_PULSE_MICROSECONDS,
        STEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS);
    stepperMotorController.initialize();
    stepperMotorController.configureRepeatingTrapezoid(
        STEPPER_MOTOR_MAX_STEPS_PER_SECOND,
        STEPPER_MOTOR_ACCELERATION_MILLISECONDS,
        STEPPER_MOTOR_CRUISE_MILLISECONDS,
        STEPPER_MOTOR_DECELERATION_MILLISECONDS);

    ExternalPowerSupplyDetector externalPowerSupplyDetector;
    externalPowerSupplyDetector.initialize();

    UartSerial serial;
    serial.initialize(ARDUINO_SERIAL_BAUD);
    serial.writeString("motionControl boot build=");
    serial.writeLine(MOTION_CONTROL_BUILD_TIMESTAMP);
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

        stepperMotorController.update(now, externalPowerSupplyAvailable);

        if ((now - lastTelemetryPrint) >= 500UL) {
            lastTelemetryPrint = now;
            serial.writeString("VIN: ");
            serial.writeVoltageMillivolts(
                externalPowerSupplyDetector.filteredExternalSupplyMillivolts());
            serial.writeString(" PSU: ");
            serial.writeString(externalPowerSupplyAvailable ? "OK" : "OFF");
            serial.writeString(" Dir: ");
            serial.writeString(
                stepperMotorController.directionForward() ? "F" : "R");
            serial.writeString(" Speed: ");
            serial.writeUnsigned(stepperMotorController.speedStepsPerSecond());
            serial.writeLine("");
        }
    }
}
