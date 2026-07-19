#ifndef __EXTERNAL_POWER_SUPPLY_DETECTOR__
#define __EXTERNAL_POWER_SUPPLY_DETECTOR__

#include <stdint.h>

class ExternalPowerSupplyDetector {
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
    void initialize();
    bool update(uint32_t nowMilliseconds);

    bool isExternalPowerSupplyAvailable();
    bool isExternalPowerSupplyAvailable(uint16_t externalSupplyMillivolts);
    uint16_t readAnalogInputMillivolts();
    uint16_t readExternalSupplyMillivolts();

    uint16_t filteredExternalSupplyMillivolts() const;
};

#endif
