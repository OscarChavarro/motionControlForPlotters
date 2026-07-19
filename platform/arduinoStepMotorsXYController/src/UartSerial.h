#ifndef __UART_SERIAL__
#define __UART_SERIAL__

#include <stdint.h>

class UartSerial {
  public:
    void initialize(uint32_t baudRate);
    bool isReadAvailable();
    char readChar();
    void writeChar(char value);
    void writeString(const char* value);
    void writeSigned(int32_t value);
    void writeUnsigned(uint32_t value);
    void writeUnsignedFixed2(uint32_t centiValue);
    void writeUnsignedFixed3(uint32_t milliValue);
    void writeVoltageMillivolts(uint16_t millivolts);
    void writeLine(const char* value);
};

#endif
