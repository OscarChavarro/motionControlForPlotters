#pragma once

#include "Element.h"

// Mirrors the firmware's external power supply detector: whether it is on
// and the voltage sensed on its input pin. Read-only: control keys have no
// effect.
class PowerSupplyUnitElement : public Element {
 public:
  PowerSupplyUnitElement(int id, bool available);

  bool isAvailable() const;
  int voltageMillivolts() const;

  std::string title() const override;
  std::vector<std::string> infoLines() const override;
  void applyStatusLine(const std::string &line) override;

 private:
  bool available_;
  int voltage_millivolts_;
};
