#pragma once

#include "../model/Element.h"

#include <cstddef>
#include <memory>
#include <vector>

// Upper area shown in ELEMENTS_MODE, above the shrunk ConsoleWidget: lays
// out one bordered box per Element, delegating the actual painting to the
// per-type Ncurses*ElementRenderer classes.
class ElementsWidget {
 public:
  void draw(
      int top_row,
      int height,
      const std::vector<std::unique_ptr<Element>> &elements,
      size_t selected_index) const;
};
