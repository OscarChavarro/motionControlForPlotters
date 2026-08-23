#include "ServoElement.h"

#include "CsvLine.h"

#include <algorithm>
#include <cstdlib>
#include <ncurses.h>

static const int SERVO_POSITION_MIN_VALUE = 0;
static const int SERVO_POSITION_MAX_VALUE = 255;
static const size_t SERVO_POSITION_MAX_LENGTH = 3U;

static int clampServoPosition(int value) {
  if (value < SERVO_POSITION_MIN_VALUE) {
    return SERVO_POSITION_MIN_VALUE;
  }
  if (value > SERVO_POSITION_MAX_VALUE) {
    return SERVO_POSITION_MAX_VALUE;
  }
  return value;
}

ServoElement::ServoElement(int id)
    : Element(id),
      position_(0),
      pulse_microseconds_(1000),
      test_enabled_(true),
      selected_widget_index_(POSITION_INPUT_WIDGET),
      position_input_text_("128"),
      position_input_touched_(false) {
}

int ServoElement::position() const {
  return position_;
}

int ServoElement::pulseMicroseconds() const {
  return pulse_microseconds_;
}

bool ServoElement::testEnabled() const {
  return test_enabled_;
}

const std::string &ServoElement::positionInputText() const {
  return position_input_text_;
}

int ServoElement::selectedWidgetIndex() const {
  return selected_widget_index_;
}

std::string ServoElement::title() const {
  return titleWithPrefix("Servo " + std::to_string(id()));
}

std::vector<std::string> ServoElement::infoLines() const {
  return {
      "Position: " + std::to_string(position_),
      "Pulse: " + std::to_string(pulse_microseconds_) + " us",
      std::string("Test: ") + (test_enabled_ ? "ON" : "OFF"),
  };
}

void ServoElement::applyStatusLine(const std::string &line) {
  std::vector<std::string> fields = splitCsvLine(line);
  if (fields.size() < 3 || fields[0] != "SERVO") {
    return;
  }

  position_ = clampServoPosition(atoi(fields[1].c_str()));
  pulse_microseconds_ = atoi(fields[2].c_str());
  if (fields.size() >= 4) {
    test_enabled_ = (fields[3] == "1");
  }
}

int ServoElement::widgetCount() const {
  return 2;
}

int ServoElement::parsedPositionValue() const {
  if (position_input_text_.empty()) {
    return 0;
  }
  return clampServoPosition(atoi(position_input_text_.c_str()));
}

bool ServoElement::handlePositionInputKey(int key) {
  if (key == KEY_BACKSPACE || key == 127 || key == '\b') {
    if (!position_input_text_.empty()) {
      position_input_text_.pop_back();
    }
    position_input_touched_ = true;
    return true;
  }

  if (key >= '0' && key <= '9') {
    if (!position_input_touched_) {
      position_input_text_.clear();
      position_input_touched_ = true;
    }
    if (position_input_text_.size() < SERVO_POSITION_MAX_LENGTH) {
      position_input_text_ += static_cast<char>(key);
    }
    if (!position_input_text_.empty()) {
      position_input_text_ = std::to_string(
          clampServoPosition(atoi(position_input_text_.c_str())));
    }
    return true;
  }

  return false;
}

bool ServoElement::handleControlKey(int key, std::string &out_command) {
  if (selected_widget_index_ >= widgetCount()) {
    selected_widget_index_ = widgetCount() - 1;
  }

  if (key == KEY_LEFT) {
    if (selected_widget_index_ > 0) {
      --selected_widget_index_;
    }
    return true;
  }

  if (key == KEY_RIGHT) {
    if (selected_widget_index_ + 1 < widgetCount()) {
      ++selected_widget_index_;
    }
    return true;
  }

  if (selected_widget_index_ == POSITION_INPUT_WIDGET &&
      handlePositionInputKey(key)) {
    return true;
  }

  if (key == ' ' && selected_widget_index_ == SET_BUTTON_WIDGET) {
    out_command = "servo " + std::to_string(id()) + " " +
        std::to_string(parsedPositionValue());
    test_enabled_ = false;
    return true;
  }

  return false;
}
