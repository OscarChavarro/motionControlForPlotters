#include "AvrUsartSerial.h"

#include <avr/io.h>

AvrUsartSerial::AvrUsartSerial(uint8_t usartIndex) : m_usartIndex(usartIndex)
{
}

void
AvrUsartSerial::initialize(uint32_t baudRate)
{
    const uint16_t ubrrValue =
        static_cast<uint16_t>((F_CPU / (8UL * baudRate)) - 1UL);

    switch (m_usartIndex) {
        case 0U:
            UBRR0H = static_cast<uint8_t>(ubrrValue >> 8U);
            UBRR0L = static_cast<uint8_t>(ubrrValue & 0xFFU);
            UCSR0A = static_cast<uint8_t>(1U << U2X0);
            UCSR0B = static_cast<uint8_t>((1U << TXEN0) | (1U << RXEN0));
            UCSR0C = static_cast<uint8_t>((1U << UCSZ01) | (1U << UCSZ00));
            break;
        case 1U:
            UBRR1H = static_cast<uint8_t>(ubrrValue >> 8U);
            UBRR1L = static_cast<uint8_t>(ubrrValue & 0xFFU);
            UCSR1A = static_cast<uint8_t>(1U << U2X1);
            UCSR1B = static_cast<uint8_t>((1U << TXEN1) | (1U << RXEN1));
            UCSR1C = static_cast<uint8_t>((1U << UCSZ11) | (1U << UCSZ10));
            break;
        case 2U:
            UBRR2H = static_cast<uint8_t>(ubrrValue >> 8U);
            UBRR2L = static_cast<uint8_t>(ubrrValue & 0xFFU);
            UCSR2A = static_cast<uint8_t>(1U << U2X2);
            UCSR2B = static_cast<uint8_t>((1U << TXEN2) | (1U << RXEN2));
            UCSR2C = static_cast<uint8_t>((1U << UCSZ21) | (1U << UCSZ20));
            break;
        default:
            UBRR3H = static_cast<uint8_t>(ubrrValue >> 8U);
            UBRR3L = static_cast<uint8_t>(ubrrValue & 0xFFU);
            UCSR3A = static_cast<uint8_t>(1U << U2X3);
            UCSR3B = static_cast<uint8_t>((1U << TXEN3) | (1U << RXEN3));
            UCSR3C = static_cast<uint8_t>((1U << UCSZ31) | (1U << UCSZ30));
            break;
    }
}

bool
AvrUsartSerial::isReadAvailable() const
{
    switch (m_usartIndex) {
        case 0U:
            return (UCSR0A & static_cast<uint8_t>(1U << RXC0)) != 0U;
        case 1U:
            return (UCSR1A & static_cast<uint8_t>(1U << RXC1)) != 0U;
        case 2U:
            return (UCSR2A & static_cast<uint8_t>(1U << RXC2)) != 0U;
        default:
            return (UCSR3A & static_cast<uint8_t>(1U << RXC3)) != 0U;
    }
}

char
AvrUsartSerial::readChar()
{
    switch (m_usartIndex) {
        case 0U:
            return static_cast<char>(UDR0);
        case 1U:
            return static_cast<char>(UDR1);
        case 2U:
            return static_cast<char>(UDR2);
        default:
            return static_cast<char>(UDR3);
    }
}

void
AvrUsartSerial::writeChar(char value)
{
    switch (m_usartIndex) {
        case 0U:
            while ((UCSR0A & static_cast<uint8_t>(1U << UDRE0)) == 0U) {
            }
            UDR0 = static_cast<uint8_t>(value);
            break;
        case 1U:
            while ((UCSR1A & static_cast<uint8_t>(1U << UDRE1)) == 0U) {
            }
            UDR1 = static_cast<uint8_t>(value);
            break;
        case 2U:
            while ((UCSR2A & static_cast<uint8_t>(1U << UDRE2)) == 0U) {
            }
            UDR2 = static_cast<uint8_t>(value);
            break;
        default:
            while ((UCSR3A & static_cast<uint8_t>(1U << UDRE3)) == 0U) {
            }
            UDR3 = static_cast<uint8_t>(value);
            break;
    }
}

uint8_t
AvrUsartSerial::usartIndex() const
{
    return m_usartIndex;
}

uint8_t
AvrUsartSerial::rxPin() const
{
    return rxPinForUsartIndex(m_usartIndex);
}

uint8_t
AvrUsartSerial::txPin() const
{
    return txPinForUsartIndex(m_usartIndex);
}

uint8_t
AvrUsartSerial::rxPinForUsartIndex(uint8_t usartIndex)
{
    switch (usartIndex) {
        case 0U:
            return 0U;
        case 1U:
            return 19U;
        case 2U:
            return 17U;
        default:
            return 15U;
    }
}

uint8_t
AvrUsartSerial::txPinForUsartIndex(uint8_t usartIndex)
{
    switch (usartIndex) {
        case 0U:
            return 1U;
        case 1U:
            return 18U;
        case 2U:
            return 16U;
        default:
            return 14U;
    }
}
