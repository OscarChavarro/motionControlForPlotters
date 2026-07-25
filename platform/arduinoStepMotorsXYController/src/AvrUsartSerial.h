#ifndef ARDUINO_STEP_MOTORS_AVR_USART_SERIAL_H
#define ARDUINO_STEP_MOTORS_AVR_USART_SERIAL_H

#include "hal/UartSerial.h"

#include <stdint.h>

// UartSerial over one of the Mega2560's 4 hardware USARTs, selected by
// index in the constructor. Unlike AvrUartSerial (which always targets
// USART0, used by the console), this lets a TMC2209 in UART mode use any
// free USART -- for example USART1 (RX1=D19, TX1=D18) -- while USART0
// stays dedicated to the console.
//
// The RX/TX pins are fixed per USART by the ATmega2560 silicon, not
// independently selectable; the index is what the constructor accepts, and
// rxPin()/txPin() report the resulting Arduino Mega digital pin numbers so
// callers (e.g. the "hardware" command) can list the resources in use.
class AvrUsartSerial : public UartSerial {
  private:
    uint8_t m_usartIndex;

  public:
    explicit AvrUsartSerial(uint8_t usartIndex);

    void initialize(uint32_t baudRate) override;
    bool isReadAvailable() const override;
    char readChar() override;
    void writeChar(char value) override;

    uint8_t usartIndex() const;
    uint8_t rxPin() const;
    uint8_t txPin() const;

    static uint8_t rxPinForUsartIndex(uint8_t usartIndex);
    static uint8_t txPinForUsartIndex(uint8_t usartIndex);
};

#endif
