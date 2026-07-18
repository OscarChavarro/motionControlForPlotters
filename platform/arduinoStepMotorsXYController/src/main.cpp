#include "BuildInfo.h"
#include "ExternalPowerSupplyDetector.h"
#include "GpioLed.h"
#include "SystemClock.h"
#include "UartSerial.h"

static bool
isCommand(const char* command, const char* expected)
{
    uint8_t index = 0;

    while (expected[index] != '\0') {
        if (command[index] != expected[index]) {
            return false;
        }
        ++index;
    }

    return command[index] == '\0';
}

int
main()
{
    SystemClock::initialize();
    GpioLed led;
    led.initialize();

    UartSerial serial;
    serial.initialize(ARDUINO_SERIAL_BAUD);
    serial.writeString("motionControl Arduino boot (build ");
    serial.writeString(MOTION_CONTROL_BUILD_TIMESTAMP);
    serial.writeLine(")");
    serial.writeLine("blink example started");

    ExternalPowerSupplyDetector externalPowerSupplyDetector;
    externalPowerSupplyDetector.initialize();

    uint32_t lastToggle = SystemClock::millis();
    uint32_t lastPsuErrorPrint = SystemClock::millis() -
        static_cast<uint32_t>(PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL);
    bool ledState = false;
    bool previousExternalPowerSupplyAvailable = false;
    bool verboseMessages = true;
    char commandBuffer[32];
    uint8_t commandBufferLength = 0;

    //noinspection CppDFAEndlessLoop
    // NOLINTNEXTLINE(bugprone-infinite-loop)
    for (;;) {
        const uint32_t now = SystemClock::millis();
        if ((now - lastToggle) >= 1000UL) {
            lastToggle = now;
            ledState = !ledState;
            led.write(ledState);
            if (verboseMessages) {
                serial.writeLine(ledState ? "LED=ON" : "LED=OFF");
            }
        }
        const uint16_t externalSupplyMillivolts =
            externalPowerSupplyDetector.readExternalSupplyMillivolts();
        const bool externalPowerSupplyAvailable =
            externalPowerSupplyDetector.isExternalPowerSupplyAvailable(externalSupplyMillivolts);

        while (serial.isReadAvailable()) {
            const char input = serial.readChar();

            if (input == '\r' || input == '\n') {
                commandBuffer[commandBufferLength] = '\0';
                if (isCommand(commandBuffer, "voltage")) {
                    serial.writeString("Voltage: ");
                    serial.writeVoltageMillivolts(externalSupplyMillivolts);
                    serial.writeLine("");
                }
                else if (isCommand(commandBuffer, "verbose off")) {
                    verboseMessages = false;
                    serial.writeLine("Verbose: off");
                }
                else if (isCommand(commandBuffer, "verbose on")) {
                    verboseMessages = true;
                    serial.writeLine("Verbose: on");
                }
                commandBufferLength = 0;
            }
            else if (input == '\b' || input == 127) {
                if (commandBufferLength > 0U) {
                    --commandBufferLength;
                }
            }
            else if (commandBufferLength < (sizeof(commandBuffer) - 1U)) {
                commandBuffer[commandBufferLength] = input;
                ++commandBufferLength;
            }
        }

        if (!externalPowerSupplyAvailable &&
            (previousExternalPowerSupplyAvailable ||
            ((now - lastPsuErrorPrint) >=
                static_cast<uint32_t>(PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL)))) {
            lastPsuErrorPrint = now;
            serial.writeString("ERROR: External power supply not found or turned off. Detected voltage: ");
            serial.writeVoltageMillivolts(externalSupplyMillivolts);
            serial.writeLine("");
        }
        else if (externalPowerSupplyAvailable &&
            !previousExternalPowerSupplyAvailable) {
            serial.writeString("External power supply found! Detected voltage: ");
            serial.writeVoltageMillivolts(externalSupplyMillivolts);
            serial.writeLine("");
        }
        previousExternalPowerSupplyAvailable = externalPowerSupplyAvailable;
    }
}
