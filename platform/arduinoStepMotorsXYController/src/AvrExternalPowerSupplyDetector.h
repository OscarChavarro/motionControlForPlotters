#ifndef ARDUINO_STEP_MOTORS_AVR_EXTERNAL_POWER_SUPPLY_DETECTOR_H
#define ARDUINO_STEP_MOTORS_AVR_EXTERNAL_POWER_SUPPLY_DETECTOR_H

#include "hal/ExternalPowerSupplyDetector.h"

class AvrExternalPowerSupplyDetector : public ExternalPowerSupplyDetector {
  private:
    static uint16_t readAdc0();
    static uint16_t minimumExternalSupplyMillivolts();
    static uint16_t disconnectExternalSupplyMillivolts();

    uint32_t m_lastSampleMilliseconds;
    uint16_t m_rawExternalSupplyMillivolts;
    uint16_t m_filteredExternalSupplyMillivolts;
    uint8_t m_consecutiveGoodSamples;
    uint8_t m_consecutiveBadSamples;
    bool m_externalPowerSupplyAvailable;
    bool m_hasSample;

  public:
    AvrExternalPowerSupplyDetector();
    ~AvrExternalPowerSupplyDetector() = default;

    void initialize() override;
    bool update(uint32_t nowMilliseconds) override;

    bool isExternalPowerSupplyAvailable() const override;
    bool isExternalPowerSupplyAvailable(
        uint16_t externalSupplyMillivolts) const override;
    uint16_t readAnalogInputMillivolts() override;
    uint16_t readExternalSupplyMillivolts() override;
    uint16_t filteredExternalSupplyMillivolts() const override;
};

#endif
