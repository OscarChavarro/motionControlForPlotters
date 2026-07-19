#include "AvrStepDirectionDriver.h"

#include <avr/io.h>
#include <util/delay.h>

AvrStepDirectionDriver::AvrStepDirectionDriver()
    : m_stepPin(0),
      m_directionPin(0),
      m_stepPulseMicroseconds(0),
      m_directionSetupMicroseconds(0),
      m_stepOutputRegister(nullptr),
      m_directionOutputRegister(nullptr),
      m_stepBitMask(0),
      m_directionBitMask(0)
{
}

void
AvrStepDirectionDriver::configure(
    uint8_t stepPin,
    uint8_t directionPin,
    uint16_t stepPulseMicroseconds,
    uint16_t directionSetupMicroseconds)
{
    m_stepPin = stepPin;
    m_directionPin = directionPin;
    m_stepPulseMicroseconds = stepPulseMicroseconds;
    m_directionSetupMicroseconds = directionSetupMicroseconds;
}

bool
AvrStepDirectionDriver::initialize()
{
    m_stepOutputRegister = outputRegisterForPin(m_stepPin);
    m_directionOutputRegister = outputRegisterForPin(m_directionPin);
    volatile uint8_t* const stepDirectionRegister =
        directionRegisterForPin(m_stepPin);
    volatile uint8_t* const motorDirectionRegister =
        directionRegisterForPin(m_directionPin);
    m_stepBitMask = bitMaskForPin(m_stepPin);
    m_directionBitMask = bitMaskForPin(m_directionPin);

    if (m_stepOutputRegister == nullptr ||
        m_directionOutputRegister == nullptr ||
        stepDirectionRegister == nullptr ||
        motorDirectionRegister == nullptr ||
        m_stepBitMask == 0U || m_directionBitMask == 0U) {
        return false;
    }

    *stepDirectionRegister =
        static_cast<uint8_t>(*stepDirectionRegister | m_stepBitMask);
    *motorDirectionRegister =
        static_cast<uint8_t>(*motorDirectionRegister | m_directionBitMask);
    writePin(m_stepOutputRegister, m_stepBitMask, false);
    writePin(m_directionOutputRegister, m_directionBitMask, false);
    return true;
}

void
AvrStepDirectionDriver::setDirection(bool forward)
{
    writePin(m_directionOutputRegister, m_directionBitMask, forward);
    delayMicroseconds(m_directionSetupMicroseconds);
}

void
AvrStepDirectionDriver::pulseStep()
{
    writePin(m_stepOutputRegister, m_stepBitMask, true);
    delayMicroseconds(m_stepPulseMicroseconds);
    writePin(m_stepOutputRegister, m_stepBitMask, false);
    delayMicroseconds(m_stepPulseMicroseconds);
}

void
AvrStepDirectionDriver::delayMicroseconds(uint16_t microseconds)
{
    for (uint16_t i = 0; i < microseconds; ++i) {
        _delay_us(1);
    }
}

void
AvrStepDirectionDriver::writePin(
    volatile uint8_t* outputRegister,
    uint8_t bitMask,
    bool high)
{
    if (outputRegister == nullptr || bitMask == 0U) {
        return;
    }
    if (high) {
        *outputRegister = static_cast<uint8_t>(*outputRegister | bitMask);
    }
    else {
        *outputRegister = static_cast<uint8_t>(*outputRegister & ~bitMask);
    }
}

volatile uint8_t*
AvrStepDirectionDriver::outputRegisterForPin(uint8_t pin)
{
#if defined(ARDUINO_AVR_MEGA2560)
    if (pin <= 3U || pin == 5U) {
        return &PORTE;
    }
    if (pin == 4U || (pin >= 39U && pin <= 41U)) {
        return &PORTG;
    }
    if ((pin >= 6U && pin <= 9U) || (pin >= 16U && pin <= 17U)) {
        return &PORTH;
    }
    if ((pin >= 10U && pin <= 13U) || (pin >= 50U && pin <= 53U)) {
        return &PORTB;
    }
    if (pin >= 14U && pin <= 15U) {
        return &PORTJ;
    }
    if ((pin >= 18U && pin <= 21U) || pin == 38U) {
        return &PORTD;
    }
    if (pin >= 22U && pin <= 29U) {
        return &PORTA;
    }
    if (pin >= 30U && pin <= 37U) {
        return &PORTC;
    }
    if (pin >= 42U && pin <= 49U) {
        return &PORTL;
    }
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO)
    if (pin <= 7U) {
        return &PORTD;
    }
    if (pin <= 13U) {
        return &PORTB;
    }
#else
#error "AvrStepDirectionDriver pin mapping is not defined for this board"
#endif
    return nullptr;
}

volatile uint8_t*
AvrStepDirectionDriver::directionRegisterForPin(uint8_t pin)
{
#if defined(ARDUINO_AVR_MEGA2560)
    if (pin <= 3U || pin == 5U) {
        return &DDRE;
    }
    if (pin == 4U || (pin >= 39U && pin <= 41U)) {
        return &DDRG;
    }
    if ((pin >= 6U && pin <= 9U) || (pin >= 16U && pin <= 17U)) {
        return &DDRH;
    }
    if ((pin >= 10U && pin <= 13U) || (pin >= 50U && pin <= 53U)) {
        return &DDRB;
    }
    if (pin >= 14U && pin <= 15U) {
        return &DDRJ;
    }
    if ((pin >= 18U && pin <= 21U) || pin == 38U) {
        return &DDRD;
    }
    if (pin >= 22U && pin <= 29U) {
        return &DDRA;
    }
    if (pin >= 30U && pin <= 37U) {
        return &DDRC;
    }
    if (pin >= 42U && pin <= 49U) {
        return &DDRL;
    }
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO)
    if (pin <= 7U) {
        return &DDRD;
    }
    if (pin <= 13U) {
        return &DDRB;
    }
#else
#error "AvrStepDirectionDriver pin mapping is not defined for this board"
#endif
    return nullptr;
}

uint8_t
AvrStepDirectionDriver::bitMaskForPin(uint8_t pin)
{
#if defined(ARDUINO_AVR_MEGA2560)
    static const uint8_t bitByPin[] = {
        PE0, PE1, PE4, PE5, PG5, PE3, PH3, PH4, PH5, PH6,
        PB4, PB5, PB6, PB7, PJ1, PJ0, PH1, PH0, PD3, PD2,
        PD1, PD0, PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7,
        PC7, PC6, PC5, PC4, PC3, PC2, PC1, PC0, PD7, PG2,
        PG1, PG0, PL7, PL6, PL5, PL4, PL3, PL2, PL1, PL0,
        PB3, PB2, PB1, PB0
    };
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO)
    static const uint8_t bitByPin[] = {
        PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7,
        PB0, PB1, PB2, PB3, PB4, PB5
    };
#else
#error "AvrStepDirectionDriver pin mapping is not defined for this board"
#endif
    if (pin >= sizeof(bitByPin)) {
        return 0U;
    }
    return static_cast<uint8_t>(1U << bitByPin[pin]);
}
