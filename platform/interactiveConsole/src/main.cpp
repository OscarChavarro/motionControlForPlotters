#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <clocale>
#include <chrono>
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

class SerialPort {
 public:
  SerialPort(const char *path, int baud_rate)
      : file_descriptor_(-1), opened_(false) {
    open(path, baud_rate);
  }

  ~SerialPort() {
    closePort();
  }

  SerialPort(const SerialPort &) = delete;
  SerialPort &operator=(const SerialPort &) = delete;

  bool open(const char *path, int baud_rate) {
    closePort();
    error_.clear();

    file_descriptor_ = ::open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (file_descriptor_ < 0) {
      error_ = std::string("Could not open ") + path + ": " + strerror(errno);
      return false;
    }

    termios options;
    if (tcgetattr(file_descriptor_, &options) != 0) {
      error_ = std::string("Could not read serial configuration: ") +
               strerror(errno);
      closePort();
      return false;
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
      closePort();
      return false;
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
      closePort();
      return false;
    }

#ifdef __APPLE__
    if (use_custom_baud_rate) {
      speed_t custom_speed = static_cast<speed_t>(baud_rate);
      if (ioctl(file_descriptor_, IOSSIOSPEED, &custom_speed) != 0) {
        error_ = std::string("Could not configure serial speed: ") +
                 strerror(errno);
        closePort();
        return false;
      }
    }
#else
    static_cast<void>(use_custom_baud_rate);
#endif

    tcflush(file_descriptor_, TCIFLUSH);
    opened_ = true;
    return true;
  }

  void closePort() {
    if (file_descriptor_ >= 0) {
      close(file_descriptor_);
      file_descriptor_ = -1;
    }
    opened_ = false;
  }

  bool isOpen() const {
    return opened_;
  }

  const std::string &error() const {
    return error_;
  }

  std::string readAvailable(bool &connection_lost) {
    std::string output;
    std::array<char, 1024> buffer;
    connection_lost = false;

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
      connection_lost = true;
      closePort();
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
#ifdef B250000
      case 250000:
        speed = B250000;
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
static const short ERROR_TEXT_PAIR = 3;
static const std::chrono::seconds RECONNECT_INTERVAL(2);

static void handleSignal(int) {
  keep_running = 0;
}

// Top area of the screen: header line, received-message scrollback with a
// scrollbar, and a bottom separator line.
class ConsoleWidget {
 public:
  void appendSerialText(const std::string &text) {
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

  void appendErrorLine(const std::string &message) {
    if (!partial_line_.empty()) {
      pushLine(partial_line_, false);
      partial_line_.clear();
    }
    pushLine(message, true);
  }

  void scrollPageUp(int visible_rows) {
    scroll_offset_ += static_cast<size_t>(visible_rows);
    clampScrollOffset(visible_rows);
  }

  void scrollPageDown(int visible_rows) {
    size_t page_size = static_cast<size_t>(visible_rows);
    scroll_offset_ = (scroll_offset_ > page_size) ? scroll_offset_ - page_size : 0U;
    clampScrollOffset(visible_rows);
  }

  void draw(int height, const std::string &status) {
    if (height < 3) {
      return;
    }

    const int separator_row = height - 1;
    const int available_rows = separator_row - 1;
    clampScrollOffset(available_rows);

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

    const int text_width = std::max(0, COLS - 2);
    const int scrollbar_column = COLS - 1;
    const size_t total_lines = totalDisplayLineCount();
    const size_t first_visible_line = firstVisibleLine(available_rows);

    int row = 1;
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
        1,
        separator_row - 1,
        scrollbar_column,
        total_lines,
        available_rows,
        first_visible_line);

    mvhline(separator_row, 0, 0, COLS);
  }

 private:
  static const size_t MESSAGE_BUFFER_LIMIT_BYTES = 100U * 1024U * 1024U;

  void pushLine(const std::string &line, bool is_error) {
    lines_.push_back(line);
    line_is_error_.push_back(is_error);
    stored_bytes_ += line.size() + 1U;
    while (!lines_.empty() && stored_bytes_ > MESSAGE_BUFFER_LIMIT_BYTES) {
      stored_bytes_ -= lines_.front().size() + 1U;
      lines_.pop_front();
      line_is_error_.pop_front();
    }
  }

  size_t totalDisplayLineCount() const {
    return lines_.size() + (partial_line_.empty() ? 0U : 1U);
  }

  const std::string &storedLineAt(size_t index) const {
    if (index < lines_.size()) {
      return lines_[index];
    }
    return partial_line_;
  }

  bool storedLineIsErrorAt(size_t index) const {
    if (index < lines_.size()) {
      return line_is_error_[index];
    }
    return false;
  }

  size_t firstVisibleLine(int available_rows) const {
    const size_t total_lines = totalDisplayLineCount();
    if (total_lines <= static_cast<size_t>(available_rows)) {
      return 0U;
    }
    return total_lines - static_cast<size_t>(available_rows) - scroll_offset_;
  }

  void clampScrollOffset(int available_rows) {
    const size_t total_lines = totalDisplayLineCount();
    if (total_lines <= static_cast<size_t>(available_rows)) {
      scroll_offset_ = 0U;
      return;
    }
    scroll_offset_ = std::min(
        scroll_offset_,
        total_lines - static_cast<size_t>(available_rows));
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

  std::deque<std::string> lines_;
  std::deque<bool> line_is_error_;
  size_t stored_bytes_ = 0U;
  std::string partial_line_;
  size_t scroll_offset_ = 0U;
};

// Middle area: a single reverse-video bar showing the available key
// shortcuts.
class KeyGuideWidget {
 public:
  void draw(int row) const {
    attron(A_REVERSE);
    mvhline(row, 0, ' ', COLS);
    mvprintw(row, 2, "<esc> exit");
    attroff(A_REVERSE);
  }
};

// Bottom area: the single-line prompt where the user types commands that
// get sent to the serial device.
class InteractiveCommandsInputWidget {
 public:
  bool handleKey(int key) {
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

  const std::string &takeCommand() {
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

  void navigateHistoryUp() {
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

  void navigateHistoryDown() {
    if (command_history_.empty() || history_index_ >= command_history_.size()) {
      return;
    }
    ++history_index_;
    input_line_ = (history_index_ == command_history_.size()) ?
        history_draft_ :
        command_history_[history_index_];
    cursor_index_ = input_line_.size();
  }

  void draw(int row) const {
    const int prompt_column = 2;
    const int text_column = prompt_column + 2;
    const int available_width = std::max(0, COLS - text_column - 1);
    int visible_start = 0;

    if (available_width > 0 && static_cast<int>(cursor_index_) >= available_width) {
      visible_start = static_cast<int>(cursor_index_) - available_width + 1;
    }

    attron(COLOR_PAIR(DEFAULT_TEXT_PAIR));
    mvhline(row, 0, ' ', COLS);
    mvprintw(row, prompt_column, "> ");
    attroff(COLOR_PAIR(DEFAULT_TEXT_PAIR));

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

 private:
  std::string input_line_;
  size_t cursor_index_ = 0U;
  std::string history_draft_;
  std::vector<std::string> command_history_;
  size_t history_index_ = 0U;
  std::string submitted_command_;
};

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
  init_pair(ERROR_TEXT_PAIR, COLOR_RED, -1);

  ConsoleWidget console;
  KeyGuideWidget key_guide;
  InteractiveCommandsInputWidget command_input;

  std::string status = std::string(port) + " " + std::to_string(baud_rate) +
                       " baud";
  bool connected = true;
  std::chrono::steady_clock::time_point next_reconnect_attempt;

  while (keep_running) {
    if (connected) {
      bool connection_lost = false;
      std::string serial_text = serial.readAvailable(connection_lost);
      if (connection_lost) {
        console.appendErrorLine("[serial error: connection lost, retrying]");
        connected = false;
        next_reconnect_attempt =
            std::chrono::steady_clock::now() + RECONNECT_INTERVAL;
      } else if (!serial_text.empty()) {
        console.appendSerialText(serial_text);
      }
    } else if (std::chrono::steady_clock::now() >= next_reconnect_attempt) {
      if (!serial.open(port, baud_rate)) {
        next_reconnect_attempt =
            std::chrono::steady_clock::now() + RECONNECT_INTERVAL;
      } else {
        connected = true;
      }
    }

    const int key_guide_height = 1;
    const int input_height = 1;
    const int console_height =
        std::max(3, LINES - key_guide_height - input_height);
    const int visible_rows = std::max(1, console_height - 2);

    int key = getch();
    if (key == 27) {
      break;
    }
    if (command_input.handleKey(key)) {
      std::string message_to_send = command_input.takeCommand() + "\n";
      if (connected && !serial.writeText(message_to_send)) {
        console.appendSerialText("[serial write error]\n");
      }
    } else if (key == KEY_PPAGE) {
      console.scrollPageUp(visible_rows);
    } else if (key == KEY_NPAGE) {
      console.scrollPageDown(visible_rows);
    } else if (key == KEY_DOWN) {
      command_input.navigateHistoryDown();
    } else if (key == KEY_UP) {
      command_input.navigateHistoryUp();
    } else if (key == KEY_RESIZE) {
      clear();
    }

    erase();
    console.draw(console_height, status);
    key_guide.draw(console_height);
    command_input.draw(console_height + key_guide_height);
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
