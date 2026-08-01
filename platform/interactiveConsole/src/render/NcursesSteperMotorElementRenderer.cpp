#include "NcursesSteperMotorElementRenderer.h"

#include "../model/SteperMotorElement.h"
#include "ElementBoxPainter.h"

bool NcursesSteperMotorElementRenderer::draw(
    const Element &element,
    int top_row,
    int left_col,
    int width,
    int height,
    bool selected) {
  const SteperMotorElement *motor =
      dynamic_cast<const SteperMotorElement *>(&element);
  if (motor == nullptr) {
    return false;
  }

  const std::vector<std::string> lines = {
      motor->referenceName(),
      "Driver: " + motor->driverDescription(),
      std::string("Dir: ") + (motor->directionForward() ? "F" : "R"),
      "Speed: " + std::to_string(motor->speedMilliStepsPerSecond()) + " mstep/s",
      "Pos: " + std::to_string(motor->positionSteps()) + " steps",
  };

  std::vector<std::vector<ElementBoxWidget>> widget_rows = {
      {{motor->testButtonLabel(), motor->selectedWidgetIndex() == 0}},
  };
  if (motor->moveByStepsVisible()) {
    widget_rows.push_back({
        {"Steps:", false, false},
        {motor->stepsInputText(), motor->selectedWidgetIndex() == 1},
    });
    widget_rows.push_back({
        {"Speed steps/sec", false, false},
        {motor->speedInputText(), motor->selectedWidgetIndex() == 2},
    });
    widget_rows.push_back({
        {"MOVE BY STEPS", motor->selectedWidgetIndex() == 3},
    });
  }

  drawElementBox(
      top_row, left_col, width, height, motor->title(), lines, selected,
      widget_rows);
  return true;
}
