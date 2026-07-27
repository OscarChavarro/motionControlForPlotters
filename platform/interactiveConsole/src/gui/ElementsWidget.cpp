#include "ElementsWidget.h"

#include "../model/ColorPairs.h"
#include "../render/NcursesPowerSupplyUnitElementRenderer.h"
#include "../render/NcursesSteperMotorElementRenderer.h"

#include <algorithm>
#include <cstring>
#include <ncurses.h>

namespace {

void drawElement(
    const Element &element,
    int top_row,
    int left_col,
    int width,
    int height,
    bool selected) {
  if (NcursesSteperMotorElementRenderer::draw(
          element, top_row, left_col, width, height, selected)) {
    return;
  }
  NcursesPowerSupplyUnitElementRenderer::draw(
      element, top_row, left_col, width, height, selected);
}

}  // namespace

void ElementsWidget::draw(
    int top_row,
    int height,
    const std::vector<std::unique_ptr<Element>> &elements,
    size_t selected_index) const {
  if (height < 1) {
    return;
  }

  mvhline(top_row, 0, 0, COLS);
  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_BOLD);
  mvprintw(top_row, 2, "Elements");
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_BOLD);

  if (height < 2) {
    return;
  }

  if (elements.empty()) {
    static const char *placeholder = "(no elements yet)";
    const int placeholder_row = top_row + height / 2;
    const int placeholder_column =
        std::max(0, (COLS - static_cast<int>(strlen(placeholder))) / 2);
    attron(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_DIM);
    mvprintw(placeholder_row, placeholder_column, "%s", placeholder);
    attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_DIM);
    return;
  }

  const int content_top = top_row + 1;
  const int content_height = height - 1;
  const int element_count = static_cast<int>(elements.size());

  // Up to 3 boxes per row; each box is drawn wider than tall (roughly
  // twice as wide as it is tall) so it reads as square on screen, since
  // terminal character cells are taller than they are wide.
  int columns = std::min(3, element_count);
  while (columns > 1 && (COLS / columns) < 16) {
    --columns;
  }
  columns = std::max(1, columns);

  const int box_width = std::max(16, COLS / columns);
  const int box_height = std::min(
      std::max(4, box_width / 2),
      std::max(4, content_height));

  for (int index = 0; index < element_count; ++index) {
    const int column = index % columns;
    const int row = index / columns;
    const int box_top = content_top + row * box_height;
    if (box_top + box_height > top_row + height) {
      break;
    }
    const int box_left = column * box_width;
    drawElement(
        *elements[static_cast<size_t>(index)],
        box_top,
        box_left,
        box_width,
        box_height,
        static_cast<size_t>(index) == selected_index);
  }
}
