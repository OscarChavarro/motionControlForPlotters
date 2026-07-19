#ifndef MOTION_CONTROL_HAL_UART_SERIAL_H
#define MOTION_CONTROL_HAL_UART_SERIAL_H

#include <stdint.h>

class UartSerial {
  protected:
    ~UartSerial() = default;

  public:
    virtual void initialize(uint32_t baudRate) = 0;
    virtual bool isReadAvailable() const = 0;
    virtual char readChar() = 0;
    virtual void writeChar(char value) = 0;

    void writeString(const char* value);
    void writeSigned(int32_t value);
    void writeUnsigned(uint32_t value);
    void writeUnsignedFixed2(uint32_t centiValue);
    void writeUnsignedFixed3(uint32_t milliValue);
    void writeVoltageMillivolts(uint16_t millivolts);
    void writeLine(const char* value);
};

#endif
