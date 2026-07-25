#include "drivers/tmc/Tmc2209Datagram.h"

#include "drivers/tmc/Tmc2209Crc.h"

namespace {

// Sync nibble + reserved bits, identical for every datagram direction
// (section 4.1: bits 0,1,2,3 = 1,0,1,0; bits 4..7 reserved = 0).
const uint8_t TMC2209_SYNC_BYTE = 0x05U;
const uint8_t TMC2209_REGISTER_WRITE_BIT = 0x80U;

} // namespace

uint8_t
Tmc2209Datagram::buildWrite(
    uint8_t nodeAddress,
    uint8_t registerAddress,
    uint32_t data,
    uint8_t outDatagram[WRITE_LENGTH])
{
    outDatagram[0] = TMC2209_SYNC_BYTE;
    outDatagram[1] = nodeAddress;
    outDatagram[2] = static_cast<uint8_t>(
        registerAddress | TMC2209_REGISTER_WRITE_BIT);
    outDatagram[3] = static_cast<uint8_t>((data >> 24) & 0xFFUL);
    outDatagram[4] = static_cast<uint8_t>((data >> 16) & 0xFFUL);
    outDatagram[5] = static_cast<uint8_t>((data >> 8) & 0xFFUL);
    outDatagram[6] = static_cast<uint8_t>(data & 0xFFUL);
    outDatagram[7] = Tmc2209Crc::calculate(outDatagram, WRITE_LENGTH - 1U);
    return WRITE_LENGTH;
}

uint8_t
Tmc2209Datagram::buildReadRequest(
    uint8_t nodeAddress,
    uint8_t registerAddress,
    uint8_t outDatagram[READ_REQUEST_LENGTH])
{
    outDatagram[0] = TMC2209_SYNC_BYTE;
    outDatagram[1] = nodeAddress;
    outDatagram[2] = registerAddress;
    outDatagram[3] =
        Tmc2209Crc::calculate(outDatagram, READ_REQUEST_LENGTH - 1U);
    return READ_REQUEST_LENGTH;
}

bool
Tmc2209Datagram::parseReadReply(
    const uint8_t datagram[READ_REPLY_LENGTH],
    uint8_t expectedRegisterAddress,
    uint32_t& outData)
{
    const uint8_t expectedCrc =
        Tmc2209Crc::calculate(datagram, READ_REPLY_LENGTH - 1U);

    if (datagram[0] != TMC2209_SYNC_BYTE) {
        return false;
    }
    if (datagram[1] != REPLY_MASTER_ADDRESS) {
        return false;
    }
    if (datagram[2] != expectedRegisterAddress) {
        return false;
    }
    if (datagram[7] != expectedCrc) {
        return false;
    }

    outData = (static_cast<uint32_t>(datagram[3]) << 24) |
        (static_cast<uint32_t>(datagram[4]) << 16) |
        (static_cast<uint32_t>(datagram[5]) << 8) |
        static_cast<uint32_t>(datagram[6]);
    return true;
}
