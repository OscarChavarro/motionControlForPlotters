#pragma once

#include "../model/Element.h"

// Paints a SteperMotorElement as a bordered box inside ElementsWidget.
class NcursesSteperMotorElementRenderer {
 public:
  // Returns false without drawing anything if `element` is not a
  // SteperMotorElement.
  static bool draw(
      const Element &element,
      int top_row,
      int left_col,
      int width,
      int height,
      bool selected);
};
