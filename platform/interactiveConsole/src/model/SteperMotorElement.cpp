#include "SteperMotorElement.h"

#include "CsvLine.h"

#include <algorithm>
#include <cstdlib>
#include <ncurses.h>

static const long STEPS_INPUT_MAX_VALUE = 10000L;
static const long STEPS_INPUT_MIN_VALUE = -10000L;
static const size_t STEPS_INPUT_MAX_LENGTH = 6U;  // "-10000"

static const long SPEED_INPUT_MIN_VALUE = 1L;
static const size_t SPEED_INPUT_MAX_LENGTH = 9U;  // "999999999"

static long
clampStepsValue(long value) {
  if (value > STEPS_INPUT_MAX_VALUE) {
    return STEPS_INPUT_MAX_VALUE;
  }
  if (value < STEPS_INPUT_MIN_VALUE) {
    return STEPS_INPUT_MIN_VALUE;
  }
  if (value == 0L) {
    return 1L;
  }
  return value;
}

static long
clampSpeedValue(long value) {
  if (value < SPEED_INPUT_MIN_VALUE) {
    return SPEED_INPUT_MIN_VALUE;
  }
  return value;
}

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
      position_steps_(0),
      test_enabled_(true),
      motor_driver_enabled_(true),
      selected_widget_index_(TEST_BUTTON_WIDGET),
      steps_input_text_("1"),
      steps_input_touched_(false),
      speed_input_text_("10"),
      speed_input_touched_(false) {
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

bool SteperMotorElement::testEnabled() const {
  return test_enabled_;
}

std::string SteperMotorElement::testButtonLabel() const {
  return test_enabled_ ? "TEST" : "SUSTAIN";
}

void SteperMotorElement::setMotorDriverEnabled(bool enabled) {
  motor_driver_enabled_ = enabled;
  if (selected_widget_index_ >= widgetCount()) {
    selected_widget_index_ = std::max(0, widgetCount() - 1);
  }
}

bool SteperMotorElement::motorDriverEnabled() const {
  return motor_driver_enabled_;
}

bool SteperMotorElement::moveByStepsVisible() const {
  return motor_driver_enabled_ && !test_enabled_;
}

const std::string &SteperMotorElement::stepsInputText() const {
  return steps_input_text_;
}

const std::string &SteperMotorElement::speedInputText() const {
  return speed_input_text_;
}

int SteperMotorElement::selectedWidgetIndex() const {
  return selected_widget_index_;
}

int SteperMotorElement::widgetCount() const {
  if (!motor_driver_enabled_) {
    return 0;
  }
  return moveByStepsVisible() ? 4 : 1;
}

long SteperMotorElement::parsedStepsValue() const {
  if (steps_input_text_.empty() || steps_input_text_ == "-") {
    return 1L;
  }
  return clampStepsValue(atol(steps_input_text_.c_str()));
}

long SteperMotorElement::parsedSpeedValue() const {
  if (speed_input_text_.empty()) {
    return 10L;
  }
  return clampSpeedValue(atol(speed_input_text_.c_str()));
}

std::string SteperMotorElement::title() const {
  return "Stepper motor " + std::to_string(index_);
}

std::vector<std::string> SteperMotorElement::infoLines() const {
  std::vector<std::string> lines = {
      reference_name_,
      "Driver: " + driver_description_,
      std::string("Dir: ") + (direction_forward_ ? "F" : "R"),
      "Speed: " + std::to_string(speed_milli_steps_per_second_) + " mstep/s",
      "Pos: " + std::to_string(position_steps_) + " steps",
  };
  if (!motor_driver_enabled_) {
    lines.push_back("MOTOR DISABLED (driver off)");
  }
  return lines;
}

void SteperMotorElement::applyStatusLine(const std::string &line) {
  std::vector<std::string> fields = splitCsvLine(line);
  if (fields.size() < 4 || fields[0] != "MOTOR") {
    return;
  }
  direction_forward_ = (fields[1] == "F");
  speed_milli_steps_per_second_ = atol(fields[2].c_str());
  position_steps_ = atol(fields[3].c_str());
  if (fields.size() >= 5) {
    test_enabled_ = (fields[4] == "1");
  }
  if (selected_widget_index_ >= widgetCount()) {
    selected_widget_index_ = std::max(0, widgetCount() - 1);
  }
}

bool SteperMotorElement::handleStepsInputKey(int key) {
  if (key == KEY_BACKSPACE || key == 127 || key == '\b') {
    if (!steps_input_text_.empty()) {
      steps_input_text_.pop_back();
    }
    steps_input_touched_ = true;
    return true;
  }

  if (key == '-') {
    if (!steps_input_touched_) {
      steps_input_text_.clear();
      steps_input_touched_ = true;
    }
    if (!steps_input_text_.empty() && steps_input_text_.front() == '-') {
      steps_input_text_.erase(steps_input_text_.begin());
    }
    else {
      steps_input_text_.insert(steps_input_text_.begin(), '-');
    }
    return true;
  }

  if (key >= '0' && key <= '9') {
    if (!steps_input_touched_) {
      steps_input_text_.clear();
      steps_input_touched_ = true;
    }
    if (steps_input_text_.size() < STEPS_INPUT_MAX_LENGTH) {
      steps_input_text_ += static_cast<char>(key);
    }
    if (steps_input_text_ != "-") {
      steps_input_text_ = std::to_string(
          clampStepsValue(atol(steps_input_text_.c_str())));
    }
    return true;
  }

  return false;
}

bool SteperMotorElement::handleSpeedInputKey(int key) {
  if (key == KEY_BACKSPACE || key == 127 || key == '\b') {
    if (!speed_input_text_.empty()) {
      speed_input_text_.pop_back();
    }
    speed_input_touched_ = true;
    return true;
  }

  if (key >= '0' && key <= '9') {
    if (!speed_input_touched_) {
      speed_input_text_.clear();
      speed_input_touched_ = true;
    }
    if (speed_input_text_.size() < SPEED_INPUT_MAX_LENGTH) {
      speed_input_text_ += static_cast<char>(key);
    }
    if (!speed_input_text_.empty()) {
      speed_input_text_ = std::to_string(
          clampSpeedValue(atol(speed_input_text_.c_str())));
    }
    return true;
  }

  return false;
}

bool SteperMotorElement::handleControlKey(int key, std::string &out_command) {
  if (!motor_driver_enabled_) {
    return false;
  }

  const int widget_count = widgetCount();
  if (selected_widget_index_ >= widget_count) {
    selected_widget_index_ = widget_count - 1;
  }

  if (key == KEY_LEFT) {
    if (selected_widget_index_ > 0) {
      --selected_widget_index_;
    }
    return true;
  }

  if (key == KEY_RIGHT) {
    if (selected_widget_index_ + 1 < widget_count) {
      ++selected_widget_index_;
    }
    return true;
  }

  if (selected_widget_index_ == STEPS_INPUT_WIDGET &&
      widget_count > STEPS_INPUT_WIDGET &&
      handleStepsInputKey(key)) {
    return true;
  }

  if (selected_widget_index_ == SPEED_INPUT_WIDGET &&
      widget_count > SPEED_INPUT_WIDGET &&
      handleSpeedInputKey(key)) {
    return true;
  }

  const bool moveByStepsButtonFocused =
      selected_widget_index_ == MOVE_BY_STEPS_BUTTON_WIDGET &&
      moveByStepsVisible();

  if (key == ' ') {
    if (selected_widget_index_ == TEST_BUTTON_WIDGET) {
      test_enabled_ = !test_enabled_;
      out_command = "test " + std::to_string(id()) +
          (test_enabled_ ? " enable" : " disable");
    }
    else if (moveByStepsButtonFocused) {
      out_command = "steps " + std::to_string(id()) + " " +
          std::to_string(parsedStepsValue()) + " " +
          std::to_string(parsedSpeedValue());
    }
    return true;
  }

  if ((key == KEY_BACKSPACE || key == 127 || key == '\b') &&
      moveByStepsButtonFocused) {
    out_command = "steps " + std::to_string(id()) + " " +
        std::to_string(-parsedStepsValue()) + " " +
        std::to_string(parsedSpeedValue());
    return true;
  }

  return false;
}
