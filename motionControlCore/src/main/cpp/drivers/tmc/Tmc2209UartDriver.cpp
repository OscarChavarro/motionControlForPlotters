#include "drivers/tmc/Tmc2209UartDriver.h"

#include "hal/UartSerial.h"

Tmc2209UartDriver::Tmc2209UartDriver(UartSerial& serial, uint8_t nodeAddress)
    : m_serial(&serial), m_nodeAddress(nodeAddress)
{
}

void
Tmc2209UartDriver::initialize(uint32_t baudRate)
{
    m_serial->initialize(baudRate);
}

void
Tmc2209UartDriver::writeRegister(uint8_t registerAddress, uint32_t data)
{
    uint8_t datagram[Tmc2209Datagram::WRITE_LENGTH];
    const uint8_t length = Tmc2209Datagram::buildWrite(
        m_nodeAddress, registerAddress, data, datagram);

    for (uint8_t i = 0U; i < length; ++i) {
        m_serial->writeChar(static_cast<char>(datagram[i]));
    }
}

bool
Tmc2209UartDriver::receiveReadReply(
    uint8_t datagram[Tmc2209Datagram::READ_REPLY_LENGTH])
{
    uint8_t receivedLength = 0U;
    uint16_t pollIteration = 0U;

    while (receivedLength < Tmc2209Datagram::READ_REPLY_LENGTH &&
        pollIteration < MAX_READ_POLL_ITERATIONS) {
        if (m_serial->isReadAvailable()) {
            datagram[receivedLength] =
                static_cast<uint8_t>(m_serial->readChar());
            ++receivedLength;
        }
        else {
            ++pollIteration;
        }
    }
    return receivedLength == Tmc2209Datagram::READ_REPLY_LENGTH;
}

bool
Tmc2209UartDriver::readRegister(uint8_t registerAddress, uint32_t& outData)
{
    uint8_t requestDatagram[Tmc2209Datagram::READ_REQUEST_LENGTH];
    const uint8_t requestLength = Tmc2209Datagram::buildReadRequest(
        m_nodeAddress, registerAddress, requestDatagram);

    for (uint8_t i = 0U; i < requestLength; ++i) {
        m_serial->writeChar(static_cast<char>(requestDatagram[i]));
    }

    uint8_t replyDatagram[Tmc2209Datagram::READ_REPLY_LENGTH];
    if (!receiveReadReply(replyDatagram)) {
        return false;
    }
    return Tmc2209Datagram::parseReadReply(
        replyDatagram, registerAddress, outData);
}

void
Tmc2209UartDriver::writeGlobalConfig(const Tmc2209GlobalConfig& config)
{
    writeRegister(TMC2209_REG_GCONF, config.toRegisterValue());
}

void
Tmc2209UartDriver::writeCurrentControl(const Tmc2209CurrentControl& control)
{
    writeRegister(TMC2209_REG_IHOLD_IRUN, control.toRegisterValue());
}

void
Tmc2209UartDriver::writeChopperConfig(const Tmc2209ChopperConfig& config)
{
    writeRegister(TMC2209_REG_CHOPCONF, config.toRegisterValue());
}

bool
Tmc2209UartDriver::readDriverStatus(Tmc2209DriverStatus& outStatus)
{
    uint32_t rawValue = 0UL;
    if (!readRegister(TMC2209_REG_DRV_STATUS, rawValue)) {
        return false;
    }
    outStatus = Tmc2209DriverStatus::fromRegisterValue(rawValue);
    return true;
}
