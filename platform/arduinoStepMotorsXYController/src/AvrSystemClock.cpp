#include "AvrSystemClock.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

volatile uint32_t AvrSystemClock::g_systemMillis = 0;

ISR(TIMER0_COMPA_vect)
{
    AvrSystemClock::onTimerCompareMatch();
}

void
AvrSystemClock::initialize()
{
    cli();

    TCCR0A = static_cast<uint8_t>(1U << WGM01);
    TCCR0B = static_cast<uint8_t>((1U << CS01) | (1U << CS00));
    OCR0A = timerCompareValue();
    TIMSK0 = static_cast<uint8_t>(1U << OCIE0A);

    sei();
}

uint32_t
AvrSystemClock::millis() const
{
    uint32_t value = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        value = g_systemMillis;
    }
    return value;
}

void
AvrSystemClock::onTimerCompareMatch()
{
    ++g_systemMillis;
}

uint8_t
AvrSystemClock::timerCompareValue()
{
    const uint32_t timerPrescaler = 64UL;
    const uint32_t timerFrequencyHz = 1000UL;
    return static_cast<uint8_t>(
        (F_CPU / timerPrescaler / timerFrequencyHz) - 1UL);
}
