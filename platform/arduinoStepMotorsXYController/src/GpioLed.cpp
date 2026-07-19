#include "GpioLed.h"

#include <avr/io.h>

void
GpioLed::initialize()
{
    *ledDdr() |= ledBitMask();
    write(false);
}

void
GpioLed::write(bool enabled)
{
    if (enabled) {
        *ledPort() |= ledBitMask();
    }
    else {
        *ledPort() &= static_cast<unsigned char>(~ledBitMask());
    }
}

volatile unsigned char*
GpioLed::ledPort()
{
    return &PORTB;
}

volatile unsigned char*
GpioLed::ledDdr()
{
    return &DDRB;
}

unsigned char
GpioLed::ledBitMask()
{
  #if defined(ARDUINO_AVR_MEGA2560)
    return static_cast<unsigned char>(1U << 7);
  #else
    return static_cast<unsigned char>(1U << 5);
  #endif
}
