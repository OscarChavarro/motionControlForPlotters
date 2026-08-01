#pragma once

#include "../model/OperationModes.h"

// Middle area: a single reverse-video bar showing the available key
// shortcuts and which operation mode is active.
class KeyGuideWidget {
 public:
  void draw(int row, OperationModes current_mode, bool element_focused) const;
};
