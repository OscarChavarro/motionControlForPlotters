#ifndef MOTION_CONTROL_DRIVERS_TMC_TMC2209_DATAGRAM_H
#define MOTION_CONTROL_DRIVERS_TMC_TMC2209_DATAGRAM_H

#include <stdint.h>

// Builds and parses the single-wire UART datagrams described in chapter 4
// "UART Single Wire Interface" of the TMC2209 datasheet.
class Tmc2209Datagram {
  public:
    static const uint8_t WRITE_LENGTH = 8U;
    static const uint8_t READ_REQUEST_LENGTH = 4U;
    static const uint8_t READ_REPLY_LENGTH = 8U;
    static const uint8_t REPLY_MASTER_ADDRESS = 0xFFU;

    // Section 4.1.1, write access datagram: sync+reserved, node address,
    // register address with the write bit set, 4 data bytes (MSB first),
    // CRC. Returns WRITE_LENGTH.
    static uint8_t buildWrite(
        uint8_t nodeAddress,
        uint8_t registerAddress,
        uint32_t data,
        uint8_t outDatagram[WRITE_LENGTH]);

    // Section 4.1.2, read access request datagram: sync+reserved, node
    // address, register address, CRC. Returns READ_REQUEST_LENGTH.
    static uint8_t buildReadRequest(
        uint8_t nodeAddress,
        uint8_t registerAddress,
        uint8_t outDatagram[READ_REQUEST_LENGTH]);

    // Section 4.1.2, read access reply datagram: sync+reserved, master
    // address (always REPLY_MASTER_ADDRESS), register address, 4 data
    // bytes (MSB first), CRC. Validates the CRC and the echoed register
    // address; returns false on mismatch.
    static bool parseReadReply(
        const uint8_t datagram[READ_REPLY_LENGTH],
        uint8_t expectedRegisterAddress,
        uint32_t& outData);
};

#endif
