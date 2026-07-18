#ifndef MOTION_CONTROL_PLATFORM_ARDUINO_UARTSERIAL_H
#define MOTION_CONTROL_PLATFORM_ARDUINO_UARTSERIAL_H

#include <stdint.h>

class UartSerial {
public:
    void initialize(uint32_t baudRate);
    void writeChar(char value);
    void writeString(const char* value);
    void writeLine(const char* value);
};

#endif
