#include "UartSerial.h"

#include <avr/io.h>

void
UartSerial::initialize(uint32_t baudRate)
{
    const uint32_t ubrrValue = (F_CPU / (8UL * baudRate)) - 1UL;

    UBRR0H = static_cast<uint8_t>(ubrrValue >> 8U);
    UBRR0L = static_cast<uint8_t>(ubrrValue & 0xFFU);
    UCSR0A = static_cast<uint8_t>(1U << U2X0);
    UCSR0B = static_cast<uint8_t>((1U << TXEN0) | (1U << RXEN0));
    UCSR0C = static_cast<uint8_t>((1U << UCSZ01) | (1U << UCSZ00));
}

bool
UartSerial::isReadAvailable()
{
    return (UCSR0A & static_cast<uint8_t>(1U << RXC0)) != 0U;
}

char
UartSerial::readChar()
{
    return static_cast<char>(UDR0);
}

void
UartSerial::writeChar(char value)
{
    while ((UCSR0A & static_cast<uint8_t>(1U << UDRE0)) == 0U) {
    }
    UDR0 = static_cast<uint8_t>(value);
}

void
UartSerial::writeString(const char* value)
{
    if (!value) {
        return;
    }
    while (*value != '\0') {
        writeChar(*value);
        ++value;
    }
}

void
UartSerial::writeSigned(int32_t value)
{
    if (value < 0L) {
        writeChar('-');
        writeUnsigned(static_cast<uint32_t>(-(value + 1L)) + 1UL);
        return;
    }
    writeUnsigned(static_cast<uint32_t>(value));
}

void
UartSerial::writeUnsigned(uint32_t value)
{
    char buffer[11];
    uint8_t index = 0;

    if (value == 0U) {
        writeChar('0');
        return;
    }

    while (value > 0U && index < sizeof(buffer)) {
        buffer[index++] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    }

    while (index > 0U) {
        writeChar(buffer[--index]);
    }
}

void
UartSerial::writeUnsignedFixed2(uint32_t centiValue)
{
    const uint32_t whole = centiValue / 100UL;
    const uint8_t fractional = static_cast<uint8_t>(centiValue % 100UL);

    writeUnsigned(whole);
    writeChar('.');
    writeChar(static_cast<char>('0' + ((fractional / 10U) % 10U)));
    writeChar(static_cast<char>('0' + (fractional % 10U)));
}

void
UartSerial::writeUnsignedFixed3(uint32_t milliValue)
{
    const uint32_t whole = milliValue / 1000UL;
    const uint16_t fractional = static_cast<uint16_t>(milliValue % 1000UL);

    writeUnsigned(whole);
    writeChar('.');
    writeChar(static_cast<char>('0' + ((fractional / 100U) % 10U)));
    writeChar(static_cast<char>('0' + ((fractional / 10U) % 10U)));
    writeChar(static_cast<char>('0' + (fractional % 10U)));
}

void
UartSerial::writeVoltageMillivolts(uint16_t millivolts)
{
    const uint16_t volts = static_cast<uint16_t>(millivolts / 1000U);
    const uint16_t fractional = static_cast<uint16_t>(millivolts % 1000U);

    writeUnsigned(volts);
    writeChar('.');
    writeChar(static_cast<char>('0' + ((fractional / 100U) % 10U)));
    writeChar(static_cast<char>('0' + ((fractional / 10U) % 10U)));
    writeChar(static_cast<char>('0' + (fractional % 10U)));
    writeChar('V');
}

void
UartSerial::writeLine(const char* value)
{
    writeString(value);
    writeString("\r\n");
}
