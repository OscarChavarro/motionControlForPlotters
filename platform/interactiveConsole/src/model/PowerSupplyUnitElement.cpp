#include "PowerSupplyUnitElement.h"

#include "CsvLine.h"

#include <cstdio>
#include <cstdlib>

PowerSupplyUnitElement::PowerSupplyUnitElement(int id, bool available)
    : Element(id),
      available_(available),
      voltage_millivolts_(0),
      motor_driver_enabled_(true) {
}

bool PowerSupplyUnitElement::isAvailable() const {
  return available_;
}

int PowerSupplyUnitElement::voltageMillivolts() const {
  return voltage_millivolts_;
}

bool PowerSupplyUnitElement::motorDriverEnabled() const {
  return motor_driver_enabled_;
}

bool PowerSupplyUnitElement::motorsActuallyEnabled() const {
  return available_ && motor_driver_enabled_;
}

std::string PowerSupplyUnitElement::motorDriverButtonLabel() const {
  return motor_driver_enabled_ ? "DRIVER ON" : "DRIVER OFF";
}

std::string PowerSupplyUnitElement::title() const {
  return "Power supply unit";
}

std::vector<std::string> PowerSupplyUnitElement::infoLines() const {
  char voltage_text[16];
  snprintf(
      voltage_text,
      sizeof(voltage_text),
      "%d.%02dV",
      voltage_millivolts_ / 1000,
      (voltage_millivolts_ % 1000) / 10);

  return {
      std::string("State: ") + (available_ ? "ON" : "OFF"),
      std::string("Voltage: ") + voltage_text,
      std::string("Motor driver: ") + (motor_driver_enabled_ ? "ON" : "OFF"),
  };
}

void PowerSupplyUnitElement::applyStatusLine(const std::string &line) {
  std::vector<std::string> fields = splitCsvLine(line);
  if (fields.size() < 3 || fields[0] != "PSU") {
    return;
  }
  available_ = (fields[1] == "ON");
  voltage_millivolts_ = atoi(fields[2].c_str());
  if (fields.size() >= 4) {
    motor_driver_enabled_ = (fields[3] == "1");
  }
}

bool PowerSupplyUnitElement::handleControlKey(
    int key, std::string &out_command) {
  if (key == ' ') {
    motor_driver_enabled_ = !motor_driver_enabled_;
    out_command = "motordriver " + std::to_string(id()) +
        (motor_driver_enabled_ ? " enable" : " disable");
    return true;
  }
  return false;
}
