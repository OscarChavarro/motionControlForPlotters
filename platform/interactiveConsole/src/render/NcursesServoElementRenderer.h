#pragma once

#include "../model/Element.h"

class NcursesServoElementRenderer {
 public:
  static bool draw(
      const Element &element,
      int top_row,
      int left_col,
      int width,
      int height,
      bool selected);
};
