#ifndef __EXTERNAL_POWER_SUPPLY_DETECTOR__
#define __EXTERNAL_POWER_SUPPLY_DETECTOR__

#include <stdint.h>

class ExternalPowerSupplyDetector {
  public:
    void initialize();
    bool isExternalPowerSupplyAvailable();
    bool isExternalPowerSupplyAvailable(uint16_t externalSupplyMillivolts);
    uint16_t readAnalogInputMillivolts();
    uint16_t readExternalSupplyMillivolts();

  private:
    static uint16_t readAdc0();
    static uint16_t minimumExternalSupplyMillivolts();
};

#endif
