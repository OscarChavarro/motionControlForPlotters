#ifndef XY_MOTION_CONTROLLER_UARTSERIAL_H
#define XY_MOTION_CONTROLLER_UARTSERIAL_H

#include <stdint.h>

class UartSerial {
public:
    void initialize(uint32_t baudRate);
    void writeChar(char value);
    void writeString(const char* value);
    void writeLine(const char* value);
};

#endif
