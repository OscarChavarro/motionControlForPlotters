#include "InteractiveCommandsInputWidget.h"

#include "../model/ColorPairs.h"

#include <algorithm>
#include <ncurses.h>

bool InteractiveCommandsInputWidget::handleKey(int key) {
  if (key == ERR) {
    return false;
  }

  if (key == KEY_LEFT) {
    if (cursor_index_ > 0U) {
      --cursor_index_;
    }
    return false;
  }

  if (key == KEY_RIGHT) {
    if (cursor_index_ < input_line_.size()) {
      ++cursor_index_;
    }
    return false;
  }

  if (key == KEY_BACKSPACE || key == 127 || key == '\b') {
    if (cursor_index_ > 0 && !input_line_.empty()) {
      input_line_.erase(cursor_index_ - 1, 1);
      --cursor_index_;
    }
    return false;
  }

  if (key == KEY_DC) {
    if (cursor_index_ < input_line_.size()) {
      input_line_.erase(cursor_index_, 1);
    }
    return false;
  }

  if (key == KEY_HOME || key == 1) {
    cursor_index_ = 0;
    return false;
  }

  if (key == KEY_END || key == 5) {
    cursor_index_ = input_line_.size();
    return false;
  }

  if (key == 21) {
    input_line_.clear();
    cursor_index_ = 0U;
    return false;
  }

  if (key == '\n' || key == '\r' || key == KEY_ENTER) {
    return true;
  }

  if (key >= 32 && key <= 126) {
    input_line_.insert(cursor_index_, 1, static_cast<char>(key));
    ++cursor_index_;
  }

  return false;
}

const std::string &InteractiveCommandsInputWidget::takeCommand() {
  if (!input_line_.empty()) {
    command_history_.push_back(input_line_);
  }
  submitted_command_ = input_line_;
  input_line_.clear();
  history_draft_.clear();
  history_index_ = command_history_.size();
  cursor_index_ = 0;
  return submitted_command_;
}

void InteractiveCommandsInputWidget::navigateHistoryUp() {
  if (command_history_.empty() || history_index_ == 0U) {
    return;
  }
  if (history_index_ == command_history_.size()) {
    history_draft_ = input_line_;
  }
  --history_index_;
  input_line_ = command_history_[history_index_];
  cursor_index_ = input_line_.size();
}

void InteractiveCommandsInputWidget::navigateHistoryDown() {
  if (command_history_.empty() || history_index_ >= command_history_.size()) {
    return;
  }
  ++history_index_;
  input_line_ = (history_index_ == command_history_.size()) ?
      history_draft_ :
      command_history_[history_index_];
  cursor_index_ = input_line_.size();
}

void InteractiveCommandsInputWidget::draw(int row, bool focused) const {
  const int prompt_column = 2;
  const int text_column = prompt_column + 2;
  const int available_width = std::max(0, COLS - text_column - 1);
  int visible_start = 0;

  if (available_width > 0 && static_cast<int>(cursor_index_) >= available_width) {
    visible_start = static_cast<int>(cursor_index_) - available_width + 1;
  }

  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR));
  mvhline(row, 0, ' ', COLS);
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR));

  const int prompt_pair = focused ? SELECTED_ELEMENT_PAIR : DEFAULT_TEXT_PAIR;
  attron(COLOR_PAIR(prompt_pair));
  mvprintw(row, prompt_column, "> ");
  attroff(COLOR_PAIR(prompt_pair));

  if (available_width <= 0) {
    return;
  }

  std::string visible_text = input_line_.substr(
      static_cast<size_t>(visible_start),
      static_cast<size_t>(available_width));
  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR));
  mvaddnstr(row, text_column, visible_text.c_str(), available_width);
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR));

  int cursor_column = text_column + static_cast<int>(cursor_index_) - visible_start;
  cursor_column = std::max(text_column, std::min(COLS - 1, cursor_column));

  const bool cursor_on_character =
      static_cast<size_t>(visible_start) + static_cast<size_t>(cursor_column - text_column) <
      input_line_.size();
  const char cursor_character =
      cursor_on_character ?
          input_line_[static_cast<size_t>(visible_start) +
                      static_cast<size_t>(cursor_column - text_column)] :
          ' ';
  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_UNDERLINE);
  mvaddch(row, cursor_column, cursor_character);
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_UNDERLINE);
}
