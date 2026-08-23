#include "NcursesServoElementRenderer.h"

#include "../model/ServoElement.h"
#include "ElementBoxPainter.h"

bool NcursesServoElementRenderer::draw(
    const Element &element,
    int top_row,
    int left_col,
    int width,
    int height,
    bool selected) {
  const ServoElement *servo = dynamic_cast<const ServoElement *>(&element);
  if (servo == nullptr) {
    return false;
  }

  const std::vector<std::string> lines = {
      "Position: " + std::to_string(servo->position()),
      "Pulse: " + std::to_string(servo->pulseMicroseconds()) + " us",
      std::string("Test: ") + (servo->testEnabled() ? "ON" : "OFF"),
  };

  const std::vector<std::vector<ElementBoxWidget>> widget_rows = {
      {
          {"Position:", false, false},
          {servo->positionInputText(), servo->selectedWidgetIndex() == 0},
      },
      {
          {"SET", servo->selectedWidgetIndex() == 1},
      },
  };

  drawElementBox(
      top_row, left_col, width, height, servo->title(), lines, selected,
      widget_rows);
  return true;
}
