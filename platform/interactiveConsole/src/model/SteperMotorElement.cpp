#include "SteperMotorElement.h"

#include "CsvLine.h"

#include <cstdlib>

SteperMotorElement::SteperMotorElement(
    int id,
    int index,
    const std::string &referenceName,
    const std::string &driverDescription)
    : Element(id),
      index_(index),
      reference_name_(referenceName),
      driver_description_(driverDescription),
      direction_forward_(true),
      speed_milli_steps_per_second_(0),
      position_steps_(0) {
}

int SteperMotorElement::index() const {
  return index_;
}

const std::string &SteperMotorElement::referenceName() const {
  return reference_name_;
}

const std::string &SteperMotorElement::driverDescription() const {
  return driver_description_;
}

bool SteperMotorElement::directionForward() const {
  return direction_forward_;
}

long SteperMotorElement::speedMilliStepsPerSecond() const {
  return speed_milli_steps_per_second_;
}

long SteperMotorElement::positionSteps() const {
  return position_steps_;
}

std::string SteperMotorElement::title() const {
  return "Stepper motor " + std::to_string(index_);
}

std::vector<std::string> SteperMotorElement::infoLines() const {
  return {
      reference_name_,
      "Driver: " + driver_description_,
      std::string("Dir: ") + (direction_forward_ ? "F" : "R"),
      "Speed: " + std::to_string(speed_milli_steps_per_second_) + " mstep/s",
      "Pos: " + std::to_string(position_steps_) + " steps",
  };
}

void SteperMotorElement::applyStatusLine(const std::string &line) {
  std::vector<std::string> fields = splitCsvLine(line);
  if (fields.size() < 4 || fields[0] != "MOTOR") {
    return;
  }
  direction_forward_ = (fields[1] == "F");
  speed_milli_steps_per_second_ = atol(fields[2].c_str());
  position_steps_ = atol(fields[3].c_str());
}

bool SteperMotorElement::handleControlKey(int key) {
  // The firmware does not yet expose a per-motor "set" command (only
  // read-only "get <id>" status). This hook is wired up so the selection
  // machinery is in place; it will start consuming keys once a control
  // command exists on the firmware side.
  static_cast<void>(key);
  return false;
}
