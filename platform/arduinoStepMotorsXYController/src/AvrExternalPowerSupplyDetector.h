#ifndef ARDUINO_STEP_MOTORS_AVR_EXTERNAL_POWER_SUPPLY_DETECTOR_H
#define ARDUINO_STEP_MOTORS_AVR_EXTERNAL_POWER_SUPPLY_DETECTOR_H

#include "hal/ExternalPowerSupplyDetector.h"

// Reads presence of the external VMOT supply through a PC817 opto-isolator
// wired to A5: VMOT lights the opto LED through a series resistor, and the
// phototransistor pulls A5 to GND while lit. This is a binary presence
// signal, not a voltage measurement, so the millivolt-returning methods
// report a nominal EXTERNAL_VOLTAGE_PSU (or 0) rather than a measured value.
class AvrExternalPowerSupplyDetector : public ExternalPowerSupplyDetector {
  private:
    static volatile uint8_t* analogInputPinRegister();
    static volatile uint8_t* analogInputPortRegister();
    static volatile uint8_t* analogInputDdrRegister();
    static uint8_t analogInputBitMask();

    // True when the opto-isolator's phototransistor is conducting (VMOT
    // present), i.e. the pin reads LOW despite its internal pull-up.
    static bool readExternalSupplyPresent();

    uint32_t m_lastSampleMilliseconds;
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
        uint16_t externalSupplyMilliVolts) const override;
    uint16_t readAnalogInputMilliVolts() override;
    uint16_t readExternalSupplyMilliVolts() override;
    uint16_t filteredExternalSupplyMilliVolts() const override;
};

#endif
