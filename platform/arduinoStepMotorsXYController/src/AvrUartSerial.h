#ifndef ARDUINO_STEP_MOTORS_AVR_UART_SERIAL_H
#define ARDUINO_STEP_MOTORS_AVR_UART_SERIAL_H

#include "hal/UartSerial.h"

class AvrUartSerial : public UartSerial {
  public:
    ~AvrUartSerial() = default;

    void initialize(uint32_t baudRate) override;
    bool isReadAvailable() const override;
    char readChar() override;
    void writeChar(char value) override;
};

#endif
