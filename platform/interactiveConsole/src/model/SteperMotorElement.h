#pragma once

#include "Element.h"

// Mirrors one entry of the firmware's stepper motor registry, plus the
// live status reported by "get <id>" (direction, speed, step count).
class SteperMotorElement : public Element {
 public:
  SteperMotorElement(
      int id,
      int index,
      const std::string &referenceName,
      const std::string &driverDescription);

  int index() const;
  const std::string &referenceName() const;
  const std::string &driverDescription() const;
  bool directionForward() const;
  long speedMilliStepsPerSecond() const;
  long positionSteps() const;

  std::string title() const override;
  std::vector<std::string> infoLines() const override;
  void applyStatusLine(const std::string &line) override;
  bool handleControlKey(int key) override;

 private:
  int index_;
  std::string reference_name_;
  std::string driver_description_;
  bool direction_forward_;
  long speed_milli_steps_per_second_;
  long position_steps_;
};
