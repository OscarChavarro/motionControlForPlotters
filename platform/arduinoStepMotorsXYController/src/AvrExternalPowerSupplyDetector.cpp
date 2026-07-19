#include "AvrExternalPowerSupplyDetector.h"

#include <avr/io.h>

AvrExternalPowerSupplyDetector::AvrExternalPowerSupplyDetector()
    : m_lastSampleMilliseconds(0),
      m_rawExternalSupplyMillivolts(0),
      m_filteredExternalSupplyMillivolts(0),
      m_consecutiveGoodSamples(0),
      m_consecutiveBadSamples(0),
      m_externalPowerSupplyAvailable(false),
      m_hasSample(false)
{
}

void
AvrExternalPowerSupplyDetector::initialize()
{
    ADMUX = static_cast<uint8_t>(1U << REFS0);
    ADCSRA = static_cast<uint8_t>(
        (1U << ADEN) |
        (1U << ADPS2) |
        (1U << ADPS1) |
        (1U << ADPS0));

    m_lastSampleMilliseconds = 0;
    m_rawExternalSupplyMillivolts = 0;
    m_filteredExternalSupplyMillivolts = 0;
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
    m_rawExternalSupplyMillivolts = readExternalSupplyMillivolts();

    if (!m_hasSample) {
        m_filteredExternalSupplyMillivolts =
            m_rawExternalSupplyMillivolts;
        m_hasSample = true;
    }
    else {
        const uint32_t filteredAccumulator =
            static_cast<uint32_t>(m_filteredExternalSupplyMillivolts) * 7UL +
            m_rawExternalSupplyMillivolts + 4UL;
        m_filteredExternalSupplyMillivolts =
            static_cast<uint16_t>(filteredAccumulator / 8UL);
    }

    if (m_externalPowerSupplyAvailable) {
        m_consecutiveGoodSamples = 0;
        if (m_filteredExternalSupplyMillivolts <
            disconnectExternalSupplyMillivolts()) {
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
        if (m_filteredExternalSupplyMillivolts >=
            minimumExternalSupplyMillivolts()) {
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
    uint16_t externalSupplyMillivolts) const
{
    return externalSupplyMillivolts >= minimumExternalSupplyMillivolts();
}

uint16_t
AvrExternalPowerSupplyDetector::readAnalogInputMillivolts()
{
    const uint32_t adcReferenceMillivolts = 5000UL;
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(readAdc0()) * adcReferenceMillivolts) / 1023UL);
}

uint16_t
AvrExternalPowerSupplyDetector::readExternalSupplyMillivolts()
{
    const uint32_t analogInputMillivolts = readAnalogInputMillivolts();
    const uint32_t vinResistorOhms =
        static_cast<uint32_t>(EXTERNAL_PSU_VOLTAGE_DIVIDER_VIN_RESISTOR_OHMS);
    const uint32_t gndResistorOhms =
        static_cast<uint32_t>(EXTERNAL_PSU_VOLTAGE_DIVIDER_GND_RESISTOR_OHMS);

    if (gndResistorOhms == 0UL) {
        return 0;
    }

    uint32_t externalSupplyMillivolts =
        (analogInputMillivolts * (vinResistorOhms + gndResistorOhms)) /
        gndResistorOhms;
    if (externalSupplyMillivolts > 65535UL) {
        externalSupplyMillivolts = 65535UL;
    }
    return static_cast<uint16_t>(externalSupplyMillivolts);
}

uint16_t
AvrExternalPowerSupplyDetector::filteredExternalSupplyMillivolts() const
{
    return m_filteredExternalSupplyMillivolts;
}

uint16_t
AvrExternalPowerSupplyDetector::readAdc0()
{
    ADMUX = static_cast<uint8_t>((ADMUX & 0xF0U) | 0U);
    ADCSRA = static_cast<uint8_t>(ADCSRA | (1U << ADSC));
    while ((ADCSRA & static_cast<uint8_t>(1U << ADSC)) != 0U) {
    }
    return ADC;
}

uint16_t
AvrExternalPowerSupplyDetector::minimumExternalSupplyMillivolts()
{
    const int32_t minimumMillivolts =
        static_cast<int32_t>(
            (EXTERNAL_VOLTAGE_PSU - EXTERNAL_VOLTAGE_PSU_TOLERANCE) * 1000.0);
    if (minimumMillivolts <= 0L) {
        return 0;
    }
    if (minimumMillivolts > 65535L) {
        return 65535U;
    }
    return static_cast<uint16_t>(minimumMillivolts);
}

uint16_t
AvrExternalPowerSupplyDetector::disconnectExternalSupplyMillivolts()
{
    const uint16_t hysteresisMillivolts = 400U;
    const uint16_t minimumMillivolts = minimumExternalSupplyMillivolts();
    if (minimumMillivolts <= hysteresisMillivolts) {
        return 0;
    }
    return static_cast<uint16_t>(minimumMillivolts - hysteresisMillivolts);
}
