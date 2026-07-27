#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Bottom area: the single-line prompt where the user types commands that
// get sent to the serial device.
class InteractiveCommandsInputWidget {
 public:
  // Returns true when the key commits the current line (Enter).
  bool handleKey(int key);

  // Consumes the current input line into the command history and returns
  // the submitted text (without the trailing newline).
  const std::string &takeCommand();

  void navigateHistoryUp();
  void navigateHistoryDown();

  // `focused` paints the "> " prompt green (keyboard focus model), or the
  // default color when focus is elsewhere (an Elements widget box).
  void draw(int row, bool focused) const;

 private:
  std::string input_line_;
  size_t cursor_index_ = 0U;
  std::string history_draft_;
  std::vector<std::string> command_history_;
  size_t history_index_ = 0U;
  std::string submitted_command_;
};
