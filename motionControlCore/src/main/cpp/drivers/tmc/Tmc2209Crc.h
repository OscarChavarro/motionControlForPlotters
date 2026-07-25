#ifndef MOTION_CONTROL_DRIVERS_TMC_TMC2209_CRC_H
#define MOTION_CONTROL_DRIVERS_TMC_TMC2209_CRC_H

#include <stdint.h>

// CRC8-ATM (polynomial x^8+x^2+x^1+x^0, init 0), transmitted LSB to MSB per
// byte. Matches the TMC2209 datasheet, chapter 4.2 "CRC Calculation".
class Tmc2209Crc {
  public:
    static uint8_t calculate(const uint8_t* datagram, uint8_t length);
};

#endif
