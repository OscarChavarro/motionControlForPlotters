#include "ConsoleWidget.h"

#include "../model/ColorPairs.h"

#include <algorithm>
#include <ncurses.h>

void ConsoleWidget::appendSerialText(const std::string &text) {
  for (char character : text) {
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      pushLine(partial_line_, false);
      partial_line_.clear();
      continue;
    }
    if (character >= 32 || character == '\t') {
      partial_line_ += character;
    }
  }
}

void ConsoleWidget::appendErrorLine(const std::string &message) {
  if (!partial_line_.empty()) {
    pushLine(partial_line_, false);
    partial_line_.clear();
  }
  pushLine(message, true);
}

void ConsoleWidget::scrollPageUp(int visible_rows) {
  scroll_offset_ += static_cast<size_t>(visible_rows);
  clampScrollOffset(visible_rows);
}

void ConsoleWidget::scrollPageDown(int visible_rows) {
  size_t page_size = static_cast<size_t>(visible_rows);
  scroll_offset_ = (scroll_offset_ > page_size) ? scroll_offset_ - page_size : 0U;
  clampScrollOffset(visible_rows);
}

void ConsoleWidget::draw(int top_row, int height, const std::string &status) {
  if (height < 3) {
    return;
  }

  const int separator_row = top_row + height - 1;
  const int available_rows = height - 2;
  clampScrollOffset(available_rows);

  mvhline(top_row, 0, 0, COLS);
  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_BOLD);
  mvprintw(top_row, 2, "Received messages");
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_BOLD);
  if (!status.empty()) {
    attron(COLOR_PAIR(DEFAULT_TEXT_PAIR));
    mvprintw(top_row, std::max(20, COLS - static_cast<int>(status.size()) - 2),
             "%s", status.c_str());
    attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR));
  }

  const int text_width = std::max(0, COLS - 2);
  const int scrollbar_column = COLS - 1;
  const size_t total_lines = totalDisplayLineCount();
  const size_t first_visible_line = firstVisibleLine(available_rows);

  int row = top_row + 1;
  for (int index = 0; index < available_rows; ++index) {
    const size_t line_index = first_visible_line + static_cast<size_t>(index);
    if (line_index >= total_lines) {
      break;
    }
    const std::string &line = storedLineAt(line_index);
    const bool is_error = storedLineIsErrorAt(line_index);
    const int color_pair = is_error ? ERROR_TEXT_PAIR : SERIAL_TEXT_PAIR;
    attron(COLOR_PAIR(color_pair) | A_DIM);
    mvaddnstr(row, 1, line.c_str(), text_width);
    attroff(COLOR_PAIR(color_pair) | A_DIM);
    ++row;
  }

  drawScrollbar(
      top_row + 1,
      separator_row - top_row - 1,
      scrollbar_column,
      total_lines,
      available_rows,
      first_visible_line);

  mvhline(separator_row, 0, 0, COLS);
}

void ConsoleWidget::pushLine(const std::string &line, bool is_error) {
  lines_.push_back(line);
  line_is_error_.push_back(is_error);
  stored_bytes_ += line.size() + 1U;
  while (!lines_.empty() && stored_bytes_ > MESSAGE_BUFFER_LIMIT_BYTES) {
    stored_bytes_ -= lines_.front().size() + 1U;
    lines_.pop_front();
    line_is_error_.pop_front();
  }
}

size_t ConsoleWidget::totalDisplayLineCount() const {
  return lines_.size() + (partial_line_.empty() ? 0U : 1U);
}

const std::string &ConsoleWidget::storedLineAt(size_t index) const {
  if (index < lines_.size()) {
    return lines_[index];
  }
  return partial_line_;
}

bool ConsoleWidget::storedLineIsErrorAt(size_t index) const {
  if (index < lines_.size()) {
    return line_is_error_[index];
  }
  return false;
}

size_t ConsoleWidget::firstVisibleLine(int available_rows) const {
  const size_t total_lines = totalDisplayLineCount();
  if (total_lines <= static_cast<size_t>(available_rows)) {
    return 0U;
  }
  return total_lines - static_cast<size_t>(available_rows) - scroll_offset_;
}

void ConsoleWidget::clampScrollOffset(int available_rows) {
  const size_t total_lines = totalDisplayLineCount();
  if (total_lines <= static_cast<size_t>(available_rows)) {
    scroll_offset_ = 0U;
    return;
  }
  scroll_offset_ = std::min(
      scroll_offset_,
      total_lines - static_cast<size_t>(available_rows));
}

void ConsoleWidget::drawScrollbar(int top_row,
                                  int height,
                                  int column,
                                  size_t total_lines,
                                  int visible_rows,
                                  size_t first_visible_line) {
  static const char *up_arrow = "↑";
  static const char *down_arrow = "↓";
  static const char *track_char = "│";
  static const char *thumb_char = "┃";

  if (height < 1 || column < 0 || column >= COLS) {
    return;
  }

  if (height < 3) {
    attron(COLOR_PAIR(DEFAULT_TEXT_PAIR));
    mvaddstr(top_row, column, up_arrow);
    if (height == 2) {
      mvaddstr(top_row + 1, column, down_arrow);
    }
    attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR));
    return;
  }

  const int track_rows = std::max(1, height - 2);

  size_t thumb_length = static_cast<size_t>(track_rows);
  size_t thumb_start = 0U;
  if (total_lines > static_cast<size_t>(visible_rows) && visible_rows > 0) {
    thumb_length = std::max<size_t>(
        1U,
        (static_cast<size_t>(visible_rows) * static_cast<size_t>(track_rows)) /
            total_lines);
    thumb_length = std::min<size_t>(thumb_length, static_cast<size_t>(track_rows));

    const size_t scrollable_lines = total_lines - static_cast<size_t>(visible_rows);
    const size_t max_thumb_start = static_cast<size_t>(track_rows) - thumb_length;
    thumb_start =
        (scrollable_lines == 0U) ?
            0U :
            (first_visible_line * max_thumb_start) / scrollable_lines;
  }

  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR));
  mvaddstr(top_row, column, up_arrow);
  for (int index = 0; index < track_rows; ++index) {
    const bool inside_thumb =
        static_cast<size_t>(index) >= thumb_start &&
        static_cast<size_t>(index) < (thumb_start + thumb_length);
    mvaddstr(
        top_row + 1 + index,
        column,
        inside_thumb ? thumb_char : track_char);
  }
  mvaddstr(top_row + height - 1, column, down_arrow);
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR));
}
