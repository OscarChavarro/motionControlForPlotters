#include "hal/UartSerial.h"

#include <stdint.h>
#include <string.h>

class FakeUartSerial : public UartSerial {
  private:
    char m_output[128];
    uint8_t m_length;
    uint32_t m_baudRate;

  public:
    FakeUartSerial() : m_output(), m_length(0), m_baudRate(0)
    {
        m_output[0] = '\0';
    }

    ~FakeUartSerial() = default;

    void initialize(uint32_t baudRate) override
    {
        m_baudRate = baudRate;
    }

    bool isReadAvailable() const override
    {
        return false;
    }

    char readChar() override
    {
        return '\0';
    }

    void writeChar(char value) override
    {
        if (m_length + 1U >= sizeof(m_output)) {
            return;
        }
        m_output[m_length++] = value;
        m_output[m_length] = '\0';
    }

    const char* output() const
    {
        return m_output;
    }

    uint32_t baudRate() const
    {
        return m_baudRate;
    }
};

int
main()
{
    FakeUartSerial fakeSerial;
    UartSerial& serial = fakeSerial;

    serial.initialize(115200UL);
    serial.writeSigned(-2147483647L - 1L);
    serial.writeChar(' ');
    serial.writeUnsignedFixed2(4550UL);
    serial.writeChar(' ');
    serial.writeUnsignedFixed3(222UL);
    serial.writeChar(' ');
    serial.writeVoltageMillivolts(11600U);
    serial.writeLine("");

    if (fakeSerial.baudRate() != 115200UL) {
        return 1;
    }
    if (strcmp(
            fakeSerial.output(),
            "-2147483648 45.50 0.222 11.600V\r\n") != 0) {
        return 2;
    }
    return 0;
}
