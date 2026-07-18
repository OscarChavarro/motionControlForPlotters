#include "ExternalPowerSupplyDetector.h"

#include <avr/io.h>

void ExternalPowerSupplyDetector::initialize()
{
    ADMUX = static_cast<uint8_t>(1U << REFS0);
    ADCSRA = static_cast<uint8_t>(
        (1U << ADEN) |
        (1U << ADPS2) |
        (1U << ADPS1) |
        (1U << ADPS0));
}

bool ExternalPowerSupplyDetector::isExternalPowerSupplyAvailable()
{
    return readExternalSupplyMillivolts() >= minimumExternalSupplyMillivolts();
}

bool ExternalPowerSupplyDetector::isExternalPowerSupplyAvailable(
    uint16_t externalSupplyMillivolts)
{
    return externalSupplyMillivolts >= minimumExternalSupplyMillivolts();
}

uint16_t ExternalPowerSupplyDetector::readAnalogInputMillivolts()
{
    const uint32_t adcReferenceMillivolts = 5000UL;
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(readAdc0()) * adcReferenceMillivolts) / 1023UL);
}

uint16_t ExternalPowerSupplyDetector::readExternalSupplyMillivolts()
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

uint16_t ExternalPowerSupplyDetector::readAdc0()
{
    ADMUX = static_cast<uint8_t>((ADMUX & 0xF0U) | 0U);
    ADCSRA = static_cast<uint8_t>(ADCSRA | (1U << ADSC));
    while ((ADCSRA & static_cast<uint8_t>(1U << ADSC)) != 0U) {
    }
    return ADC;
}

uint16_t ExternalPowerSupplyDetector::minimumExternalSupplyMillivolts()
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
