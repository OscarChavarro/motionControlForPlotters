#include "AvrUartSerial.h"

#include <avr/interrupt.h>
#include <avr/io.h>

namespace {

const uint8_t RECEIVE_BUFFER_CAPACITY = 64U;
const uint8_t RECEIVE_BUFFER_INDEX_MASK = RECEIVE_BUFFER_CAPACITY - 1U;

volatile char g_receiveBuffer[RECEIVE_BUFFER_CAPACITY];
volatile uint8_t g_receiveHead = 0U;
volatile uint8_t g_receiveTail = 0U;

}  // namespace

ISR(USART0_RX_vect)
{
    AvrUartSerial::onReceiveComplete();
}

void
AvrUartSerial::initialize(uint32_t baudRate)
{
    const uint32_t ubrrValue = (F_CPU / (8UL * baudRate)) - 1UL;

    UBRR0H = static_cast<uint8_t>(ubrrValue >> 8U);
    UBRR0L = static_cast<uint8_t>(ubrrValue & 0xFFU);
    UCSR0A = static_cast<uint8_t>(1U << U2X0);
    UCSR0B = static_cast<uint8_t>(
        (1U << RXCIE0) | (1U << TXEN0) | (1U << RXEN0));
    UCSR0C = static_cast<uint8_t>((1U << UCSZ01) | (1U << UCSZ00));
}

bool
AvrUartSerial::isReadAvailable() const
{
    return g_receiveHead != g_receiveTail;
}

char
AvrUartSerial::readChar()
{
    const char value = g_receiveBuffer[g_receiveTail];
    g_receiveTail = static_cast<uint8_t>(
        (g_receiveTail + 1U) & RECEIVE_BUFFER_INDEX_MASK);
    return value;
}

void
AvrUartSerial::writeChar(char value)
{
    while ((UCSR0A & static_cast<uint8_t>(1U << UDRE0)) == 0U) {
    }
    UDR0 = static_cast<uint8_t>(value);
}

void
AvrUartSerial::onReceiveComplete()
{
    const uint8_t receivedByte = UDR0;
    const uint8_t nextHead = static_cast<uint8_t>(
        (g_receiveHead + 1U) & RECEIVE_BUFFER_INDEX_MASK);

    if (nextHead != g_receiveTail) {
        g_receiveBuffer[g_receiveHead] = static_cast<char>(receivedByte);
        g_receiveHead = nextHead;
    }
}
