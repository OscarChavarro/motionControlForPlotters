#include "NcursesPowerSupplyUnitElementRenderer.h"

#include "../model/PowerSupplyUnitElement.h"
#include "ElementBoxPainter.h"

#include <cstdio>

bool NcursesPowerSupplyUnitElementRenderer::draw(
    const Element &element,
    int top_row,
    int left_col,
    int width,
    int height,
    bool selected) {
  const PowerSupplyUnitElement *psu =
      dynamic_cast<const PowerSupplyUnitElement *>(&element);
  if (psu == nullptr) {
    return false;
  }

  char voltage_text[16];
  snprintf(
      voltage_text,
      sizeof(voltage_text),
      "%d.%02dV",
      psu->voltageMillivolts() / 1000,
      (psu->voltageMillivolts() % 1000) / 10);

  const std::vector<std::string> lines = {
      std::string("State: ") + (psu->isAvailable() ? "ON" : "OFF"),
      std::string("Volt: ") + voltage_text,
  };

  drawElementBox(top_row, left_col, width, height, psu->title(), lines, selected);
  return true;
}
