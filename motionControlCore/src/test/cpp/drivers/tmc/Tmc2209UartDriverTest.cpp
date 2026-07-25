#include "drivers/tmc/Tmc2209UartDriver.h"

#include "drivers/tmc/Tmc2209Registers.h"
#include "hal/UartSerial.h"

#include <stdint.h>
#include <string.h>

class FakeUartSerial : public UartSerial {
  private:
    uint8_t m_written[32];
    uint8_t m_writtenLength;
    uint8_t m_inbound[32];
    uint8_t m_inboundLength;
    uint8_t m_inboundReadIndex;
    uint32_t m_baudRate;

  public:
    FakeUartSerial()
        : m_written(),
          m_writtenLength(0),
          m_inbound(),
          m_inboundLength(0),
          m_inboundReadIndex(0),
          m_baudRate(0)
    {
    }

    ~FakeUartSerial() = default;

    void initialize(uint32_t baudRate) override
    {
        m_baudRate = baudRate;
    }

    bool isReadAvailable() const override
    {
        return m_inboundReadIndex < m_inboundLength;
    }

    char readChar() override
    {
        return static_cast<char>(m_inbound[m_inboundReadIndex++]);
    }

    void writeChar(char value) override
    {
        if (m_writtenLength >= sizeof(m_written)) {
            return;
        }
        m_written[m_writtenLength++] = static_cast<uint8_t>(value);
    }

    void queueInboundByte(uint8_t value)
    {
        if (m_inboundLength >= sizeof(m_inbound)) {
            return;
        }
        m_inbound[m_inboundLength++] = value;
    }

    const uint8_t*
    written() const
    {
        return m_written;
    }

    uint8_t
    writtenLength() const
    {
        return m_writtenLength;
    }

    uint32_t
    baudRate() const
    {
        return m_baudRate;
    }
};

int
main()
{
    FakeUartSerial fakeSerial;
    UartSerial& serial = fakeSerial;
    Tmc2209UartDriver driver(serial, 0U);

    driver.initialize(500000UL);
    if (fakeSerial.baudRate() != 500000UL) {
        return 1;
    }

    Tmc2209GlobalConfig config;
    config.disableStandstillCurrentReductionPin = true;
    config.microStepResolutionFromRegister = true;
    config.filterStepPulses = true;
    driver.writeGlobalConfig(config);

    const uint8_t expectedWrite[8] =
        {0x05U, 0x00U, 0x80U, 0x00U, 0x00U, 0x01U, 0xC0U, 0xF6U};
    if (fakeSerial.writtenLength() != 8U) {
        return 2;
    }
    if (memcmp(fakeSerial.written(), expectedWrite, 8U) != 0) {
        return 3;
    }

    fakeSerial.queueInboundByte(0x05U);
    fakeSerial.queueInboundByte(0xFFU);
    fakeSerial.queueInboundByte(0x6FU);
    fakeSerial.queueInboundByte(0x00U);
    fakeSerial.queueInboundByte(0x00U);
    fakeSerial.queueInboundByte(0x00U);
    fakeSerial.queueInboundByte(0x00U);
    fakeSerial.queueInboundByte(0xC6U);

    Tmc2209DriverStatus status;
    if (!driver.readDriverStatus(status)) {
        return 4;
    }
    if (status.standstill || status.overtemperaturePrewarning) {
        return 5;
    }

    return 0;
}
