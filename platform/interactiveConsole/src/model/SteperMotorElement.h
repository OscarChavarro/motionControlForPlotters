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

  // Test button state, toggled by the "space" key while this element's
  // test-button widget has focus. Mirrors the firmware's per-motor
  // "test <id> enable|disable" flag.
  bool testEnabled() const;
  std::string testButtonLabel() const;

  // Reflects the PSU element's motorsActuallyEnabled(): false whenever
  // there is no PSU, or the user turned the shared motor driver off (e.g.
  // a panic button). Propagated in from outside since this element has no
  // direct visibility into the PSU's state.
  void setMotorDriverEnabled(bool enabled);
  bool motorDriverEnabled() const;

  // The steps/speed inputs and the "MOVE BY STEPS" button only exist
  // while the motor is in SUSTAIN (test disabled) and the shared motor
  // driver is enabled: running the blocking "steps" firmware command
  // while the repeating test movement is active would fight over
  // direction and position, and neither test nor steps do anything useful
  // while the driver itself is off.
  bool moveByStepsVisible() const;
  const std::string &stepsInputText() const;
  const std::string &speedInputText() const;

  // Index of the widget currently focused within this element. Widget 0
  // (the test button) always exists; widgets 1-3 (steps input, speed
  // input, and the MOVE BY STEPS button) only exist while
  // moveByStepsVisible().
  int selectedWidgetIndex() const;

  std::string title() const override;
  std::vector<std::string> infoLines() const override;
  void applyStatusLine(const std::string &line) override;
  bool handleControlKey(int key, std::string &out_command) override;

 private:
  static const int TEST_BUTTON_WIDGET = 0;
  static const int STEPS_INPUT_WIDGET = 1;
  static const int SPEED_INPUT_WIDGET = 2;
  static const int MOVE_BY_STEPS_BUTTON_WIDGET = 3;

  int widgetCount() const;
  bool handleStepsInputKey(int key);
  bool handleSpeedInputKey(int key);
  long parsedStepsValue() const;
  long parsedSpeedValue() const;

  int index_;
  std::string reference_name_;
  std::string driver_description_;
  bool direction_forward_;
  long speed_milli_steps_per_second_;
  long position_steps_;
  bool test_enabled_;
  bool motor_driver_enabled_;
  int selected_widget_index_;
  std::string steps_input_text_;
  bool steps_input_touched_;
  std::string speed_input_text_;
  bool speed_input_touched_;
};
