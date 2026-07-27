#pragma once

#include "Element.h"

#include <memory>
#include <string>
#include <vector>

// Turns the text lines printed by the firmware's "hardware" command into
// the console's own Element model.
class HardwareReportParser {
 public:
  static std::vector<std::unique_ptr<Element>> parse(
      const std::vector<std::string> &lines);
};
