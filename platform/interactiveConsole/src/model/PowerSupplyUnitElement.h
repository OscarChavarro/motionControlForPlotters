#pragma once

#include "Element.h"

// Mirrors the firmware's external power supply detector: whether it is on,
// the voltage sensed on its input pin, and the motorDriverEnabled flag
// (defaults true; the user can turn it off as a panic button while the PSU
// is present, and it is always forced off when the PSU is not present).
class PowerSupplyUnitElement : public Element {
 public:
  PowerSupplyUnitElement(int id, bool available);

  bool isAvailable() const;
  int voltageMillivolts() const;
  bool motorDriverEnabled() const;

  // The shared motor driver is only actually driven when both the PSU is
  // present and the motorDriverEnabled flag is on.
  bool motorsActuallyEnabled() const;

  std::string motorDriverButtonLabel() const;

  std::string title() const override;
  std::vector<std::string> infoLines() const override;
  void applyStatusLine(const std::string &line) override;
  bool handleControlKey(int key, std::string &out_command) override;

 private:
  bool available_;
  int voltage_millivolts_;
  bool motor_driver_enabled_;
};
