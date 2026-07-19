#ifndef MOTION_CONTROL_HAL_EXTERNAL_POWER_SUPPLY_DETECTOR_H
#define MOTION_CONTROL_HAL_EXTERNAL_POWER_SUPPLY_DETECTOR_H

#include <stdint.h>

class ExternalPowerSupplyDetector {
  protected:
    ~ExternalPowerSupplyDetector() = default;

  public:
    virtual void initialize() = 0;
    virtual bool update(uint32_t nowMilliseconds) = 0;

    virtual bool isExternalPowerSupplyAvailable() const = 0;
    virtual bool isExternalPowerSupplyAvailable(
        uint16_t externalSupplyMillivolts) const = 0;
    virtual uint16_t readAnalogInputMillivolts() = 0;
    virtual uint16_t readExternalSupplyMillivolts() = 0;
    virtual uint16_t filteredExternalSupplyMillivolts() const = 0;
};

#endif
