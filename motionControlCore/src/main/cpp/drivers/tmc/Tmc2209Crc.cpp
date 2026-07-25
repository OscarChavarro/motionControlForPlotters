#include "drivers/tmc/Tmc2209Crc.h"

uint8_t
Tmc2209Crc::calculate(const uint8_t* datagram, uint8_t length)
{
    uint8_t crc = 0U;

    for (uint8_t byteIndex = 0U; byteIndex < length; ++byteIndex) {
        uint8_t currentByte = datagram[byteIndex];
        for (uint8_t bitIndex = 0U; bitIndex < 8U; ++bitIndex) {
            const uint8_t feedbackBit = static_cast<uint8_t>(
                (crc >> 7) ^ (currentByte & 0x01U));
            if (feedbackBit != 0U) {
                crc = static_cast<uint8_t>((crc << 1) ^ 0x07U);
            }
            else {
                crc = static_cast<uint8_t>(crc << 1);
            }
            currentByte = static_cast<uint8_t>(currentByte >> 1);
        }
    }
    return crc;
}
