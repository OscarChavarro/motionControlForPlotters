#include <avr/io.h>

#include "AvrExternalPowerSupplyDetector.h"

namespace {
const uint16_t NOMINAL_EXTERNAL_SUPPLY_MILLIVOLTS =
    static_cast<uint16_t>(EXTERNAL_VOLTAGE_PSU * 1000.0);
}

AvrExternalPowerSupplyDetector::AvrExternalPowerSupplyDetector()
    : m_lastSampleMilliseconds(0),
      m_consecutiveGoodSamples(0),
      m_consecutiveBadSamples(0),
      m_externalPowerSupplyAvailable(false),
      m_hasSample(false)
{
}

void
AvrExternalPowerSupplyDetector::initialize()
{
    // Input with the AVR's internal pull-up enabled: idles HIGH, and is
    // pulled LOW by the PC817 phototransistor when VMOT is present. No
    // external pull-up resistor is needed.
    *analogInputDdrRegister() &= static_cast<uint8_t>(~analogInputBitMask());
    *analogInputPortRegister() |= analogInputBitMask();

    m_lastSampleMilliseconds = 0;
    m_consecutiveGoodSamples = 0;
    m_consecutiveBadSamples = 0;
    m_externalPowerSupplyAvailable = false;
    m_hasSample = false;
}

bool
AvrExternalPowerSupplyDetector::update(uint32_t nowMilliseconds)
{
    const uint32_t sampleIntervalMilliseconds = 10UL;
    const uint8_t stableSampleCount = 10U;

    if (m_hasSample &&
        (nowMilliseconds - m_lastSampleMilliseconds) <
            sampleIntervalMilliseconds) {
        return false;
    }

    m_lastSampleMilliseconds = nowMilliseconds;
    m_hasSample = true;
    const bool rawPresent = readExternalSupplyPresent();

    if (m_externalPowerSupplyAvailable) {
        m_consecutiveGoodSamples = 0;
        if (!rawPresent) {
            if (m_consecutiveBadSamples < stableSampleCount) {
                ++m_consecutiveBadSamples;
            }
            if (m_consecutiveBadSamples >= stableSampleCount) {
                m_externalPowerSupplyAvailable = false;
            }
        }
        else {
            m_consecutiveBadSamples = 0;
        }
    }
    else {
        m_consecutiveBadSamples = 0;
        if (rawPresent) {
            if (m_consecutiveGoodSamples < stableSampleCount) {
                ++m_consecutiveGoodSamples;
            }
            if (m_consecutiveGoodSamples >= stableSampleCount) {
                m_externalPowerSupplyAvailable = true;
            }
        }
        else {
            m_consecutiveGoodSamples = 0;
        }
    }

    return true;
}

bool
AvrExternalPowerSupplyDetector::isExternalPowerSupplyAvailable() const
{
    return m_externalPowerSupplyAvailable;
}

bool
AvrExternalPowerSupplyDetector::isExternalPowerSupplyAvailable(
    uint16_t externalSupplyMilliVolts) const
{
    return externalSupplyMilliVolts > 0U;
}

uint16_t
AvrExternalPowerSupplyDetector::readAnalogInputMilliVolts()
{
    // The actual voltage present at the pin itself: near 0V when the
    // phototransistor conducts, near 5V from the internal pull-up when it
    // does not.
    return readExternalSupplyPresent() ? 0U : 5000U;
}

uint16_t
AvrExternalPowerSupplyDetector::readExternalSupplyMilliVolts()
{
    return readExternalSupplyPresent() ?
        NOMINAL_EXTERNAL_SUPPLY_MILLIVOLTS : 0U;
}

uint16_t
AvrExternalPowerSupplyDetector::filteredExternalSupplyMilliVolts() const
{
    return m_externalPowerSupplyAvailable ?
        NOMINAL_EXTERNAL_SUPPLY_MILLIVOLTS : 0U;
}

bool
AvrExternalPowerSupplyDetector::readExternalSupplyPresent()
{
    return (*analogInputPinRegister() & analogInputBitMask()) == 0U;
}

volatile uint8_t*
AvrExternalPowerSupplyDetector::analogInputPinRegister()
{
#if defined(ARDUINO_AVR_MEGA2560)
    return &PINF;
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO)
    return &PINC;
#else
#error "AvrExternalPowerSupplyDetector pin mapping is not defined for this board"
#endif
}

volatile uint8_t*
AvrExternalPowerSupplyDetector::analogInputPortRegister()
{
#if defined(ARDUINO_AVR_MEGA2560)
    return &PORTF;
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO)
    return &PORTC;
#else
#error "AvrExternalPowerSupplyDetector pin mapping is not defined for this board"
#endif
}

volatile uint8_t*
AvrExternalPowerSupplyDetector::analogInputDdrRegister()
{
#if defined(ARDUINO_AVR_MEGA2560)
    return &DDRF;
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO)
    return &DDRC;
#else
#error "AvrExternalPowerSupplyDetector pin mapping is not defined for this board"
#endif
}

uint8_t
AvrExternalPowerSupplyDetector::analogInputBitMask()
{
    return static_cast<uint8_t>(1U << 5);
}
