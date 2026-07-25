#include "drivers/tmc/Tmc2209StepperMotorDriver.h"

#include "drivers/tmc/Tmc2209Registers.h"
#include "hal/UartSerial.h"
#include "motion/stepperMotor/StepperMotorDriver.h"

#include <stdint.h>
#include <string.h>

class FakeStepDirectionDriver : public StepperMotorDriver {
  private:
    bool m_initializeCalled;
    bool m_forward;
    uint32_t m_forwardPulses;
    uint32_t m_backwardPulses;

  public:
    FakeStepDirectionDriver()
        : m_initializeCalled(false),
          m_forward(false),
          m_forwardPulses(0),
          m_backwardPulses(0)
    {
    }

    ~FakeStepDirectionDriver() = default;

    bool
    initialize() override
    {
        m_initializeCalled = true;
        return true;
    }

    void
    setDirection(bool forward) override
    {
        m_forward = forward;
    }

    void
    pulseStep() override
    {
        if (m_forward) {
            ++m_forwardPulses;
        }
        else {
            ++m_backwardPulses;
        }
    }

    bool
    initializeCalled() const
    {
        return m_initializeCalled;
    }

    uint32_t
    forwardPulses() const
    {
        return m_forwardPulses;
    }

    uint32_t
    backwardPulses() const
    {
        return m_backwardPulses;
    }
};

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

    void
    initialize(uint32_t baudRate) override
    {
        m_baudRate = baudRate;
    }

    bool
    isReadAvailable() const override
    {
        return m_inboundReadIndex < m_inboundLength;
    }

    char
    readChar() override
    {
        return static_cast<char>(m_inbound[m_inboundReadIndex++]);
    }

    void
    writeChar(char value) override
    {
        if (m_writtenLength >= sizeof(m_written)) {
            return;
        }
        m_written[m_writtenLength++] = static_cast<uint8_t>(value);
    }

    void
    queueInboundByte(uint8_t value)
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
    FakeStepDirectionDriver fakeStepDir;
    FakeUartSerial fakeSerial;
    UartSerial& serial = fakeSerial;

    Tmc2209StepperMotorDriver driver(fakeStepDir, serial, 0U);

    // Motion must be delegated unchanged to the wrapped STEP/DIR driver.
    StepperMotorDriver& asStepperMotorDriver = driver;
    if (!asStepperMotorDriver.initialize() || !fakeStepDir.initializeCalled()) {
        return 1;
    }
    asStepperMotorDriver.setDirection(true);
    asStepperMotorDriver.pulseStep();
    asStepperMotorDriver.pulseStep();
    if (fakeStepDir.forwardPulses() != 2U || fakeStepDir.backwardPulses() != 0U) {
        return 2;
    }

    // UART configuration must reach the transport as the expected datagram.
    driver.configureUart(500000UL);
    if (fakeSerial.baudRate() != 500000UL) {
        return 3;
    }

    Tmc2209GlobalConfig config;
    config.disableStandstillCurrentReductionPin = true;
    config.microStepResolutionFromRegister = true;
    config.filterStepPulses = true;
    driver.writeGlobalConfig(config);

    const uint8_t expectedWrite[8] =
        {0x05U, 0x00U, 0x80U, 0x00U, 0x00U, 0x01U, 0xC0U, 0xF6U};
    if (fakeSerial.writtenLength() != 8U) {
        return 4;
    }
    if (memcmp(fakeSerial.written(), expectedWrite, 8U) != 0) {
        return 5;
    }

    // readDriverStatus must parse a queued reply through to the caller.
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
        return 6;
    }
    if (status.standstill) {
        return 7;
    }

    return 0;
}
