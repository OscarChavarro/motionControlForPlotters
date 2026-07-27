#pragma once

#include <cstddef>
#include <deque>
#include <string>

// Received-message scrollback area: header line, scrolling text with a
// scrollbar, and a bottom separator line.
class ConsoleWidget {
 public:
  void appendSerialText(const std::string &text);
  void appendErrorLine(const std::string &message);

  void scrollPageUp(int visible_rows);
  void scrollPageDown(int visible_rows);

  void draw(int top_row, int height, const std::string &status);

 private:
  static const size_t MESSAGE_BUFFER_LIMIT_BYTES = 100U * 1024U * 1024U;

  void pushLine(const std::string &line, bool is_error);
  size_t totalDisplayLineCount() const;
  const std::string &storedLineAt(size_t index) const;
  bool storedLineIsErrorAt(size_t index) const;
  size_t firstVisibleLine(int available_rows) const;
  void clampScrollOffset(int available_rows);

  static void drawScrollbar(int top_row,
                            int height,
                            int column,
                            size_t total_lines,
                            int visible_rows,
                            size_t first_visible_line);

  std::deque<std::string> lines_;
  std::deque<bool> line_is_error_;
  size_t stored_bytes_ = 0U;
  std::string partial_line_;
  size_t scroll_offset_ = 0U;
};
