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

  drawElementBox(top_row, left_col, width, height, motor->title(), lines, selected);
  return true;
}
