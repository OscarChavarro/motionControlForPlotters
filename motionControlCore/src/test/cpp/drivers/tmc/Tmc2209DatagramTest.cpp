#include "drivers/tmc/Tmc2209Datagram.h"

#include "drivers/tmc/Tmc2209Registers.h"

#include <string.h>

int
main()
{
    // Write GCONF=0x000001C0 to node address 0.
    uint8_t writeDatagram[Tmc2209Datagram::WRITE_LENGTH];
    const uint8_t writeLength = Tmc2209Datagram::buildWrite(
        0U, TMC2209_REG_GCONF, 0x000001C0UL, writeDatagram);
    const uint8_t expectedWrite[8] =
        {0x05U, 0x00U, 0x80U, 0x00U, 0x00U, 0x01U, 0xC0U, 0xF6U};
    if (writeLength != 8U) {
        return 1;
    }
    if (memcmp(writeDatagram, expectedWrite, 8U) != 0) {
        return 2;
    }

    // Read request for DRV_STATUS at node address 0.
    uint8_t readRequestDatagram[Tmc2209Datagram::READ_REQUEST_LENGTH];
    const uint8_t readRequestLength = Tmc2209Datagram::buildReadRequest(
        0U, TMC2209_REG_DRV_STATUS, readRequestDatagram);
    const uint8_t expectedReadRequest[4] = {0x05U, 0x00U, 0x6FU, 0x84U};
    if (readRequestLength != 4U) {
        return 3;
    }
    if (memcmp(readRequestDatagram, expectedReadRequest, 4U) != 0) {
        return 4;
    }

    // Well-formed read reply for DRV_STATUS = 0.
    const uint8_t goodReply[8] =
        {0x05U, 0xFFU, 0x6FU, 0x00U, 0x00U, 0x00U, 0x00U, 0xC6U};
    uint32_t outData = 0xFFFFFFFFUL;
    if (!Tmc2209Datagram::parseReadReply(
            goodReply, TMC2209_REG_DRV_STATUS, outData)) {
        return 5;
    }
    if (outData != 0UL) {
        return 6;
    }

    // Corrupted CRC must be rejected.
    uint8_t corruptedReply[8];
    memcpy(corruptedReply, goodReply, 8U);
    corruptedReply[7] = static_cast<uint8_t>(corruptedReply[7] ^ 0xFFU);
    if (Tmc2209Datagram::parseReadReply(
            corruptedReply, TMC2209_REG_DRV_STATUS, outData)) {
        return 7;
    }

    // Reply for a different register than requested must be rejected.
    if (Tmc2209Datagram::parseReadReply(goodReply, TMC2209_REG_GCONF, outData)) {
        return 8;
    }

    return 0;
}
