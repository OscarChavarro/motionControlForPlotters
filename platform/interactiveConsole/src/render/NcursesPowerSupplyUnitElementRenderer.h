#pragma once

#include "../model/Element.h"

// Paints a PowerSupplyUnitElement as a bordered box inside ElementsWidget.
class NcursesPowerSupplyUnitElementRenderer {
 public:
  // Returns false without drawing anything if `element` is not a
  // PowerSupplyUnitElement.
  static bool draw(
      const Element &element,
      int top_row,
      int left_col,
      int width,
      int height,
      bool selected);
};
