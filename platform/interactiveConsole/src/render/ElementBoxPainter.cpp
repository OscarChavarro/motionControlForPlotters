#include "ElementBoxPainter.h"

#include "../model/ColorPairs.h"

#include <algorithm>
#include <ncurses.h>

void drawElementBox(
    int top_row,
    int left_col,
    int width,
    int height,
    const std::string &title,
    const std::vector<std::string> &lines,
    bool selected) {
  if (width < 3 || height < 3) {
    return;
  }

  const int frame_pair = selected ? SELECTED_ELEMENT_PAIR : UNSELECTED_ELEMENT_PAIR;
  const int right_col = left_col + width - 1;
  const int bottom_row = top_row + height - 1;

  // Only the frame (border + title) reflects selection; the body text
  // always uses the neutral color so selecting a box doesn't turn its
  // whole content green.
  attron(COLOR_PAIR(frame_pair) | (selected ? A_BOLD : A_DIM));

  mvaddch(top_row, left_col, ACS_ULCORNER);
  mvaddch(top_row, right_col, ACS_URCORNER);
  mvaddch(bottom_row, left_col, ACS_LLCORNER);
  mvaddch(bottom_row, right_col, ACS_LRCORNER);
  mvhline(top_row, left_col + 1, ACS_HLINE, width - 2);
  mvhline(bottom_row, left_col + 1, ACS_HLINE, width - 2);
  mvvline(top_row + 1, left_col, ACS_VLINE, height - 2);
  mvvline(top_row + 1, right_col, ACS_VLINE, height - 2);

  if (!title.empty() && width > 4) {
    const std::string label = " " + title + " ";
    const int max_label_width = width - 2;
    const int label_col = left_col + std::max(
        1, (width - static_cast<int>(label.size())) / 2);
    mvaddnstr(top_row, label_col, label.c_str(), max_label_width);
  }

  attroff(COLOR_PAIR(frame_pair) | (selected ? A_BOLD : A_DIM));

  const int text_width = std::max(0, width - 2);
  int row = top_row + 1;
  attron(COLOR_PAIR(UNSELECTED_ELEMENT_PAIR) | A_DIM);
  for (const std::string &line : lines) {
    if (row >= bottom_row) {
      break;
    }
    mvaddnstr(row, left_col + 1, line.c_str(), text_width);
    ++row;
  }
  attroff(COLOR_PAIR(UNSELECTED_ELEMENT_PAIR) | A_DIM);
}
