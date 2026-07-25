#include "drivers/tmc/Tmc2209Crc.h"

int
main()
{
    // Reference vectors computed independently with a Python
    // reimplementation of the same bit-serial algorithm published in
    // the TMC2209 datasheet, chapter 4.2 "CRC Calculation".

    // Write GCONF=0x000001C0 to node address 0.
    const uint8_t writeGconf[7] =
        {0x05U, 0x00U, 0x80U, 0x00U, 0x00U, 0x01U, 0xC0U};
    if (Tmc2209Crc::calculate(writeGconf, 7U) != 0xF6U) {
        return 1;
    }

    // Write IHOLD_IRUN with IHOLD=6, IRUN=13, IHOLDDELAY=4.
    const uint8_t writeIholdIrun[7] =
        {0x05U, 0x00U, 0x90U, 0x00U, 0x04U, 0x0DU, 0x06U};
    if (Tmc2209Crc::calculate(writeIholdIrun, 7U) != 0x7BU) {
        return 2;
    }

    // Read request for DRV_STATUS at node address 0.
    const uint8_t readDrvStatus[3] = {0x05U, 0x00U, 0x6FU};
    if (Tmc2209Crc::calculate(readDrvStatus, 3U) != 0x84U) {
        return 3;
    }

    // Read reply for DRV_STATUS = 0, addressed to the master (0xFF).
    const uint8_t readReply[7] =
        {0x05U, 0xFFU, 0x6FU, 0x00U, 0x00U, 0x00U, 0x00U};
    if (Tmc2209Crc::calculate(readReply, 7U) != 0xC6U) {
        return 4;
    }

    return 0;
}
