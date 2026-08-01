#include "KeyGuideWidget.h"

#include <ncurses.h>

void KeyGuideWidget::draw(
    int row, OperationModes current_mode, bool element_focused) const {
  attron(A_REVERSE);
  mvhline(row, 0, ' ', COLS);
  mvprintw(row, 2, "<esc> exit");
  mvprintw(
      row,
      16,
      "%s<F1> Console",
      current_mode == CONSOLE_MODE ? "*" : " ");
  mvprintw(
      row,
      32,
      "%s<F2> Elements",
      current_mode == ELEMENTS_MODE ? "*" : " ");
  if (current_mode == ELEMENTS_MODE) {
    mvprintw(row, 49, "<tab> Next Element");
  }
  if (current_mode == ELEMENTS_MODE && element_focused) {
    mvprintw(row, 69, "<left/right> Widget  <space> Click");
  }
  attroff(A_REVERSE);
}
