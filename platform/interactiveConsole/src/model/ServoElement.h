#pragma once

#include "Element.h"

class ServoElement : public Element {
 public:
  explicit ServoElement(int id);

  int position() const;
  int pulseMicroseconds() const;
  bool testEnabled() const;
  const std::string &positionInputText() const;
  int selectedWidgetIndex() const;

  std::string title() const override;
  std::vector<std::string> infoLines() const override;
  void applyStatusLine(const std::string &line) override;
  bool handleControlKey(int key, std::string &out_command) override;

 private:
  static const int POSITION_INPUT_WIDGET = 0;
  static const int SET_BUTTON_WIDGET = 1;

  int widgetCount() const;
  bool handlePositionInputKey(int key);
  int parsedPositionValue() const;

  int position_;
  int pulse_microseconds_;
  bool test_enabled_;
  int selected_widget_index_;
  std::string position_input_text_;
  bool position_input_touched_;
};
