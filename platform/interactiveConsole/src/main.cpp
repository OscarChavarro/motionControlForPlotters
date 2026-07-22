#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <ncurses.h>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>
#endif

struct BarControl {
  std::string label;
  int value;
};

class SerialPort {
 public:
  SerialPort(const char *path, int baud_rate)
      : file_descriptor_(-1), opened_(false) {
    file_descriptor_ = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (file_descriptor_ < 0) {
      error_ = std::string("Could not open ") + path + ": " + strerror(errno);
      return;
    }

    termios options;
    if (tcgetattr(file_descriptor_, &options) != 0) {
      error_ = std::string("Could not read serial configuration: ") +
               strerror(errno);
      close(file_descriptor_);
      file_descriptor_ = -1;
      return;
    }

    cfmakeraw(&options);
    speed_t speed;
    bool use_custom_baud_rate = false;
    if (!baudRateToSpeed(baud_rate, speed)) {
#ifdef __APPLE__
      speed = B9600;
      use_custom_baud_rate = true;
#else
      error_ = "Unsupported baud rate: " + std::to_string(baud_rate);
      close(file_descriptor_);
      file_descriptor_ = -1;
      return;
#endif
    }

    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    options.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    options.c_cflag &= static_cast<tcflag_t>(~PARENB);
    options.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
    options.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    options.c_cflag |= CS8;

    if (tcsetattr(file_descriptor_, TCSANOW, &options) != 0) {
      error_ = std::string("Could not configure serial port: ") +
               strerror(errno);
      close(file_descriptor_);
      file_descriptor_ = -1;
      return;
    }

#ifdef __APPLE__
    if (use_custom_baud_rate) {
      speed_t custom_speed = static_cast<speed_t>(baud_rate);
      if (ioctl(file_descriptor_, IOSSIOSPEED, &custom_speed) != 0) {
        error_ = std::string("Could not configure serial speed: ") +
                 strerror(errno);
        close(file_descriptor_);
        file_descriptor_ = -1;
        return;
      }
    }
#else
    static_cast<void>(use_custom_baud_rate);
#endif

    tcflush(file_descriptor_, TCIFLUSH);
    opened_ = true;
  }

  ~SerialPort() {
    if (file_descriptor_ >= 0) {
      close(file_descriptor_);
    }
  }

  SerialPort(const SerialPort &) = delete;
  SerialPort &operator=(const SerialPort &) = delete;

  bool isOpen() const {
    return opened_;
  }

  const std::string &error() const {
    return error_;
  }

  std::string readAvailable() {
    std::string output;
    std::array<char, 1024> buffer;

    while (true) {
      ssize_t bytes_read = read(file_descriptor_, buffer.data(), buffer.size());
      if (bytes_read > 0) {
        output.append(buffer.data(), static_cast<size_t>(bytes_read));
        continue;
      }
      if (bytes_read == 0) {
        break;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        break;
      }
      output += "\n[serial error: ";
      output += strerror(errno);
      output += "]\n";
      break;
    }

    return output;
  }

  bool writeText(const std::string &text) {
    if (file_descriptor_ < 0) {
      return false;
    }

    size_t offset = 0;
    while (offset < text.size()) {
      ssize_t bytes_written = write(
          file_descriptor_,
          text.data() + offset,
          text.size() - offset);
      if (bytes_written > 0) {
        offset += static_cast<size_t>(bytes_written);
        continue;
      }
      if (bytes_written < 0 && errno == EINTR) {
        continue;
      }
      return false;
    }
    return true;
  }

 private:
  static bool baudRateToSpeed(int baud_rate, speed_t &speed) {
    switch (baud_rate) {
      case 9600:
        speed = B9600;
        return true;
      case 19200:
        speed = B19200;
        return true;
      case 38400:
        speed = B38400;
        return true;
      case 57600:
        speed = B57600;
        return true;
      case 115200:
        speed = B115200;
        return true;
#ifdef B230400
      case 230400:
        speed = B230400;
        return true;
#endif
#ifdef B460800
      case 460800:
        speed = B460800;
        return true;
#endif
#ifdef B500000
      case 500000:
        speed = B500000;
        return true;
#endif
#ifdef B921600
      case 921600:
        speed = B921600;
        return true;
#endif
#ifdef B1000000
      case 1000000:
        speed = B1000000;
        return true;
#endif
      default:
        return false;
    }
  }

  int file_descriptor_;
  bool opened_;
  std::string error_;
};

static volatile sig_atomic_t keep_running = 1;
static const short DEFAULT_TEXT_PAIR = 1;
static const short SERIAL_TEXT_PAIR = 2;
static const size_t MESSAGE_BUFFER_LIMIT_BYTES = 100U * 1024U * 1024U;

static void handleSignal(int) {
  keep_running = 0;
}

static size_t totalDisplayLineCount(const std::deque<std::string> &lines,
                                    const std::string &partial_line) {
  return lines.size() + (partial_line.empty() ? 0U : 1U);
}

static const std::string &storedLineAt(const std::deque<std::string> &lines,
                                       const std::string &partial_line,
                                       size_t index) {
  if (index < lines.size()) {
    return lines[index];
  }
  return partial_line;
}

static void trimMessageBuffer(std::deque<std::string> &lines,
                              size_t &stored_bytes,
                              size_t limit_bytes) {
  while (!lines.empty() && stored_bytes > limit_bytes) {
    stored_bytes -= lines.front().size() + 1U;
    lines.pop_front();
  }
}

static void appendSerialText(std::deque<std::string> &lines,
                             size_t &stored_bytes,
                             std::string &partial_line,
                             const std::string &text,
                             size_t limit_bytes) {
  for (char character : text) {
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      lines.push_back(partial_line);
      stored_bytes += partial_line.size() + 1U;
      partial_line.clear();
      trimMessageBuffer(lines, stored_bytes, limit_bytes);
      continue;
    }
    if (character >= 32 || character == '\t') {
      partial_line += character;
    }
  }
}

static void drawScrollbar(int top_row,
                          int height,
                          int column,
                          size_t total_lines,
                          int visible_rows,
                          size_t first_visible_line) {
  static const char *up_arrow = "↑";
  static const char *down_arrow = "↓";
  static const char *track_char = "│";
  static const char *thumb_char = "┃";

  if (height < 3 || column < 0 || column >= COLS) {
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

static void drawMessages(int top_height,
                         const std::deque<std::string> &lines,
                         size_t scroll_offset,
                         const std::string &partial_line,
                         const std::string &status) {
  mvhline(0, 0, 0, COLS);
  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_BOLD);
  mvprintw(0, 2, "Received messages");
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_BOLD);
  if (!status.empty()) {
    attron(COLOR_PAIR(DEFAULT_TEXT_PAIR));
    mvprintw(0, std::max(20, COLS - static_cast<int>(status.size()) - 2),
             "%s", status.c_str());
    attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR));
  }

  int row = 1;
  const int scrollbar_column = COLS - 1;
  const int text_width = std::max(0, COLS - 2);
  int available_rows = std::max(0, top_height - 1);
  const size_t total_lines = totalDisplayLineCount(lines, partial_line);
  const size_t clamped_scroll_offset =
      (total_lines > static_cast<size_t>(available_rows)) ?
          std::min(
              scroll_offset,
              total_lines - static_cast<size_t>(available_rows)) :
          0U;
  const size_t first_visible_line =
      (total_lines > static_cast<size_t>(available_rows)) ?
          total_lines - static_cast<size_t>(available_rows) -
              clamped_scroll_offset :
          0U;

  attron(COLOR_PAIR(SERIAL_TEXT_PAIR) | A_DIM);
  for (int index = 0; index < available_rows && row < top_height; ++index) {
    const size_t line_index = first_visible_line + static_cast<size_t>(index);
    if (line_index >= total_lines) {
      break;
    }
    const std::string &line = storedLineAt(lines, partial_line, line_index);
    mvaddnstr(row, 1, line.c_str(), text_width);
    ++row;
  }
  attroff(COLOR_PAIR(SERIAL_TEXT_PAIR) | A_DIM);
  drawScrollbar(
      1,
      top_height - 1,
      scrollbar_column,
      total_lines,
      available_rows,
      first_visible_line);
}

static void drawBars(int first_row,
                     int height,
                     const std::vector<BarControl> &bars,
                     size_t selected_bar) {
  if (height < 4 || bars.empty()) {
    return;
  }

  mvhline(first_row, 0, ACS_HLINE, COLS);
  int bar_area_top = first_row + 1;
  int bar_area_height = height - 2;
  int spacing = std::max(8, COLS / static_cast<int>(bars.size()));

  for (size_t index = 0; index < bars.size(); ++index) {
    int center = spacing / 2 + static_cast<int>(index) * spacing;
    if (center >= COLS - 1) {
      break;
    }

    int filled = (bars[index].value * (bar_area_height - 2)) / 100;
    int bar_bottom = bar_area_top + bar_area_height - 3;

    if (index == selected_bar) {
      attron(A_REVERSE);
    }
    mvprintw(bar_area_top, std::max(0, center - 3), "%3d%%", bars[index].value);
    for (int row = 0; row < bar_area_height - 2; ++row) {
      int screen_row = bar_bottom - row;
      mvaddch(screen_row, center, row < filled ? ACS_CKBOARD : ACS_VLINE);
    }
    mvaddnstr(bar_area_top + bar_area_height - 2,
              std::max(0, center - static_cast<int>(bars[index].label.size()) / 2),
              bars[index].label.c_str(), COLS - center);
    if (index == selected_bar) {
      attroff(A_REVERSE);
    }
  }
}

static void drawFooter() {
  attron(A_REVERSE);
  mvhline(LINES - 1, 0, ' ', COLS);
  mvprintw(LINES - 1, 2, "<esc> exit");
  attroff(A_REVERSE);
}

static void drawInputLine(const std::string &input_line, size_t cursor_index) {
  const int input_row = LINES - 2;
  const int prompt_column = 2;
  const int text_column = prompt_column + 2;
  const int available_width = std::max(0, COLS - text_column - 1);
  int visible_start = 0;

  if (available_width > 0 && static_cast<int>(cursor_index) >= available_width) {
    visible_start = static_cast<int>(cursor_index) - available_width + 1;
  }

  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR));
  mvhline(input_row, 0, ' ', COLS);
  mvprintw(input_row, prompt_column, "> ");
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR));

  if (available_width <= 0) {
    return;
  }

  std::string visible_text = input_line.substr(
      static_cast<size_t>(visible_start),
      static_cast<size_t>(available_width));
  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR));
  mvaddnstr(input_row, text_column, visible_text.c_str(), available_width);
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR));

  int cursor_column = text_column + static_cast<int>(cursor_index) - visible_start;
  cursor_column = std::max(text_column, std::min(COLS - 1, cursor_column));

  const bool cursor_on_character =
      static_cast<size_t>(visible_start) + static_cast<size_t>(cursor_column - text_column) <
      input_line.size();
  const char cursor_character =
      cursor_on_character ?
          input_line[static_cast<size_t>(visible_start) +
                     static_cast<size_t>(cursor_column - text_column)] :
          ' ';
  attron(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_UNDERLINE);
  mvaddch(input_row, cursor_column, cursor_character);
  attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR) | A_UNDERLINE);
}

static bool handleInputEditing(int key,
                               std::string &input_line,
                               size_t &cursor_index) {
  if (key == ERR) {
    return false;
  }

  if (key == KEY_LEFT) {
    if (cursor_index > 0U) {
      --cursor_index;
    }
    return false;
  }

  if (key == KEY_RIGHT) {
    if (cursor_index < input_line.size()) {
      ++cursor_index;
    }
    return false;
  }

  if (key == KEY_BACKSPACE || key == 127 || key == '\b') {
    if (cursor_index > 0 && !input_line.empty()) {
      input_line.erase(cursor_index - 1, 1);
      --cursor_index;
    }
    return false;
  }

  if (key == KEY_DC) {
    if (cursor_index < input_line.size()) {
      input_line.erase(cursor_index, 1);
    }
    return false;
  }

  if (key == KEY_HOME || key == 1) {
    cursor_index = 0;
    return false;
  }

  if (key == KEY_END || key == 5) {
    cursor_index = input_line.size();
    return false;
  }

  if (key == 21) {
    input_line.clear();
    cursor_index = 0U;
    return false;
  }

  if (key == '\n' || key == '\r' || key == KEY_ENTER) {
    return true;
  }

  if (key >= 32 && key <= 126) {
    input_line.insert(cursor_index, 1, static_cast<char>(key));
    ++cursor_index;
  }

  return false;
}

static void navigateCommandHistoryUp(const std::vector<std::string> &command_history,
                                     std::string &history_draft,
                                     size_t &history_index,
                                     std::string &input_line,
                                     size_t &cursor_index) {
  if (command_history.empty() || history_index == 0U) {
    return;
  }

  if (history_index == command_history.size()) {
    history_draft = input_line;
  }

  --history_index;
  input_line = command_history[history_index];
  cursor_index = input_line.size();
}

static void navigateCommandHistoryDown(
    const std::vector<std::string> &command_history,
    const std::string &history_draft,
    size_t &history_index,
    std::string &input_line,
    size_t &cursor_index) {
  if (command_history.empty() || history_index >= command_history.size()) {
    return;
  }

  ++history_index;
  if (history_index == command_history.size()) {
    input_line = history_draft;
  } else {
    input_line = command_history[history_index];
  }
  cursor_index = input_line.size();
}

static int runConsole(const char *port, int baud_rate) {
  setlocale(LC_ALL, "");

  SerialPort serial(port, baud_rate);
  if (!serial.isOpen()) {
    fprintf(stderr, "%s\n", serial.error().c_str());
    return 1;
  }

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  curs_set(0);
  start_color();
  use_default_colors();
  init_pair(DEFAULT_TEXT_PAIR, COLOR_WHITE, -1);
  init_pair(SERIAL_TEXT_PAIR, COLOR_WHITE, -1);

  std::deque<std::string> lines;
  size_t stored_bytes = 0;
  std::string partial_line;
  std::string input_line;
  std::string history_draft;
  std::vector<std::string> command_history;
  std::vector<BarControl> bars = {
      {"X", 50},
      {"Y", 50},
      {"Z", 50},
      {"F", 50},
  };
  size_t selected_bar = 0;
  size_t message_scroll_offset = 0;
  size_t input_cursor_index = 0;
  size_t history_index = 0;
  std::string status = std::string(port) + " " + std::to_string(baud_rate) +
                       " baud";

  while (keep_running) {
    std::string serial_text = serial.readAvailable();
    if (!serial_text.empty()) {
      appendSerialText(
          lines,
          stored_bytes,
          partial_line,
          serial_text,
          MESSAGE_BUFFER_LIMIT_BYTES);
    }

    int key = getch();
    if (key == 27) {
      break;
    }
    if (handleInputEditing(key, input_line, input_cursor_index)) {
      std::string message_to_send = input_line + "\n";
      if (!serial.writeText(message_to_send)) {
        appendSerialText(
            lines,
            stored_bytes,
            partial_line,
            "[serial write error]\n",
            MESSAGE_BUFFER_LIMIT_BYTES);
      }
      if (!input_line.empty()) {
        command_history.push_back(input_line);
      }
      input_line.clear();
      history_draft.clear();
      history_index = command_history.size();
      input_cursor_index = 0;
    } else if (key == KEY_PPAGE) {
      int footer_height = 2;
      int bars_height = std::min(10, std::max(0, LINES / 4));
      int messages_height = std::max(3, LINES - bars_height - footer_height);
      int visible_rows = std::max(1, messages_height - 1);
      message_scroll_offset += static_cast<size_t>(visible_rows);
      size_t total_lines = totalDisplayLineCount(lines, partial_line);
      if (total_lines > static_cast<size_t>(visible_rows)) {
        message_scroll_offset = std::min(
            message_scroll_offset,
            total_lines - static_cast<size_t>(visible_rows));
      } else {
        message_scroll_offset = 0;
      }
    } else if (key == KEY_NPAGE) {
      int footer_height = 2;
      int bars_height = std::min(10, std::max(0, LINES / 4));
      int messages_height = std::max(3, LINES - bars_height - footer_height);
      int visible_rows = std::max(1, messages_height - 1);
      size_t page_size = static_cast<size_t>(visible_rows);
      message_scroll_offset =
          (message_scroll_offset > page_size) ?
              message_scroll_offset - page_size :
              0U;
    } else if (key == KEY_DOWN) {
      navigateCommandHistoryDown(
          command_history,
          history_draft,
          history_index,
          input_line,
          input_cursor_index);
    } else if (key == KEY_UP) {
      navigateCommandHistoryUp(
          command_history,
          history_draft,
          history_index,
          input_line,
          input_cursor_index);
    } else if (key == KEY_RESIZE) {
      clear();
    }

    erase();
    int footer_height = 2;
    int bars_height = std::min(10, std::max(0, LINES / 4));
    int messages_height = std::max(3, LINES - bars_height - footer_height);
    int visible_rows = std::max(1, messages_height - 1);
    size_t total_lines = totalDisplayLineCount(lines, partial_line);
    if (total_lines <= static_cast<size_t>(visible_rows)) {
      message_scroll_offset = 0;
    } else {
      message_scroll_offset = std::min(
          message_scroll_offset,
          total_lines - static_cast<size_t>(visible_rows));
    }
    drawMessages(
        messages_height,
        lines,
        message_scroll_offset,
        partial_line,
        status);
    drawBars(messages_height, bars_height, bars, selected_bar);
    drawInputLine(input_line, input_cursor_index);
    drawFooter();
    refresh();

    napms(30);
  }

  endwin();
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <serial-port> <baud-rate>\n", argv[0]);
    return 2;
  }

  char *end = nullptr;
  long baud_rate = strtol(argv[2], &end, 10);
  if (end == argv[2] || *end != '\0' || baud_rate <= 0) {
    fprintf(stderr, "Invalid baud rate: %s\n", argv[2]);
    return 2;
  }

  signal(SIGINT, handleSignal);
  signal(SIGTERM, handleSignal);

  return runConsole(argv[1], static_cast<int>(baud_rate));
}
