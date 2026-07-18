#include "platform/arduino/GpioLed.h"
#include "platform/arduino/SystemClock.h"
#include "platform/arduino/UartSerial.h"

int main()
{
    SystemClock::initialize();
    GpioLed led;
    led.initialize();

    UartSerial serial;
    serial.initialize(ARDUINO_SERIAL_BAUD);
    serial.writeLine("motionControl Arduino boot");
    serial.writeLine("blink example started");

    uint32_t lastToggle = SystemClock::millis();
    bool ledState = false;

    //noinspection CppDFAEndlessLoop
    // NOLINTNEXTLINE(bugprone-infinite-loop)
    for (;;) {
        const uint32_t now = SystemClock::millis();
        if ((now - lastToggle) >= 1000UL) {
            lastToggle = now;
            ledState = !ledState;
            led.write(ledState);
            serial.writeLine(ledState ? "LED=ON" : "LED=OFF");
        }
    }
}
