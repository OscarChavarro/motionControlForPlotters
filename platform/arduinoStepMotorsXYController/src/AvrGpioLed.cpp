#include "AvrGpioLed.h"

#include <avr/io.h>

void
AvrGpioLed::initialize()
{
    *ledDdr() |= ledBitMask();
    write(false);
}

void
AvrGpioLed::write(bool enabled)
{
    if (enabled) {
        *ledPort() |= ledBitMask();
    }
    else {
        *ledPort() &= static_cast<unsigned char>(~ledBitMask());
    }
}

volatile unsigned char*
AvrGpioLed::ledPort()
{
    return &PORTB;
}

volatile unsigned char*
AvrGpioLed::ledDdr()
{
    return &DDRB;
}

unsigned char
AvrGpioLed::ledBitMask()
{
  #if defined(ARDUINO_AVR_MEGA2560)
    return static_cast<unsigned char>(1U << 7);
  #else
    return static_cast<unsigned char>(1U << 5);
  #endif
}
