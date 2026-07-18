#include "platform/arduino/UartSerial.h"

#include <avr/io.h>

void UartSerial::initialize(uint32_t baudRate)
{
    const uint32_t ubrrValue = (F_CPU / (16UL * baudRate)) - 1UL;

    UBRR0H = static_cast<uint8_t>(ubrrValue >> 8U);
    UBRR0L = static_cast<uint8_t>(ubrrValue & 0xFFU);
    UCSR0A = 0;
    UCSR0B = static_cast<uint8_t>((1U << TXEN0) | (1U << RXEN0));
    UCSR0C = static_cast<uint8_t>((1U << UCSZ01) | (1U << UCSZ00));
}

void UartSerial::writeChar(char value)
{
    while ((UCSR0A & static_cast<uint8_t>(1U << UDRE0)) == 0U) {
    }
    UDR0 = static_cast<uint8_t>(value);
}

void UartSerial::writeString(const char* value)
{
    if (!value) {
        return;
    }
    while (*value != '\0') {
        writeChar(*value);
        ++value;
    }
}

void UartSerial::writeLine(const char* value)
{
    writeString(value);
    writeString("\r\n");
}
