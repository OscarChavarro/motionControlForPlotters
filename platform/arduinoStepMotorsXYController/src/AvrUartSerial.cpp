#include "AvrUartSerial.h"

#include <avr/io.h>

void
AvrUartSerial::initialize(uint32_t baudRate)
{
    const uint32_t ubrrValue = (F_CPU / (8UL * baudRate)) - 1UL;

    UBRR0H = static_cast<uint8_t>(ubrrValue >> 8U);
    UBRR0L = static_cast<uint8_t>(ubrrValue & 0xFFU);
    UCSR0A = static_cast<uint8_t>(1U << U2X0);
    UCSR0B = static_cast<uint8_t>((1U << TXEN0) | (1U << RXEN0));
    UCSR0C = static_cast<uint8_t>((1U << UCSZ01) | (1U << UCSZ00));
}

bool
AvrUartSerial::isReadAvailable() const
{
    return (UCSR0A & static_cast<uint8_t>(1U << RXC0)) != 0U;
}

char
AvrUartSerial::readChar()
{
    return static_cast<char>(UDR0);
}

void
AvrUartSerial::writeChar(char value)
{
    while ((UCSR0A & static_cast<uint8_t>(1U << UDRE0)) == 0U) {
    }
    UDR0 = static_cast<uint8_t>(value);
}
