#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
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
      error_ = std::string("No se pudo abrir ") + path + ": " + strerror(errno);
      return;
    }

    termios options;
    if (tcgetattr(file_descriptor_, &options) != 0) {
      error_ = std::string("No se pudo leer configuracion serie: ") +
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
      error_ = "Baudios no soportados: " + std::to_string(baud_rate);
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
      error_ = std::string("No se pudo configurar el puerto serie: ") +
               strerror(errno);
      close(file_descriptor_);
      file_descriptor_ = -1;
      return;
    }

#ifdef __APPLE__
    if (use_custom_baud_rate) {
      speed_t custom_speed = static_cast<speed_t>(baud_rate);
      if (ioctl(file_descriptor_, IOSSIOSPEED, &custom_speed) != 0) {
        error_ = std::string("No se pudo configurar la velocidad serie: ") +
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
      output += "\n[error serie: ";
      output += strerror(errno);
      output += "]\n";
      break;
    }

    return output;
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

static void handleSignal(int) {
  keep_running = 0;
}

static void appendSerialText(std::deque<std::string> &lines,
                             std::string &partial_line,
                             const std::string &text,
                             size_t max_lines) {
  for (char character : text) {
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      lines.push_back(partial_line);
      partial_line.clear();
      while (lines.size() > max_lines) {
        lines.pop_front();
      }
      continue;
    }
    if (character >= 32 || character == '\t') {
      partial_line += character;
    }
  }
}

static void drawMessages(int top_height,
                         const std::deque<std::string> &lines,
                         const std::string &partial_line,
                         const std::string &status) {
  mvhline(0, 0, 0, COLS);
  attron(A_BOLD);
  mvprintw(0, 2, "Mensajes recibidos");
  attroff(A_BOLD);
  if (!status.empty()) {
    mvprintw(0, std::max(20, COLS - static_cast<int>(status.size()) - 2),
             "%s", status.c_str());
  }
  mvhline(1, 0, ACS_HLINE, COLS);

  int row = 2;
  int available_rows = std::max(0, top_height - 3);
  int stored_count = static_cast<int>(lines.size());
  int start = std::max(0, stored_count - available_rows);
  for (int index = start; index < stored_count && row < top_height; ++index) {
    mvaddnstr(row, 1, lines[static_cast<size_t>(index)].c_str(), COLS - 2);
    ++row;
  }

  if (!partial_line.empty() && row < top_height) {
    mvaddnstr(row, 1, partial_line.c_str(), COLS - 2);
  }
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
  mvprintw(LINES - 1, 2, "<esc> salir");
  attroff(A_REVERSE);
}

static int runConsole(const char *port, int baud_rate) {
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
  use_default_colors();

  std::deque<std::string> lines;
  std::string partial_line;
  std::vector<BarControl> bars = {
      {"X", 50},
      {"Y", 50},
      {"Z", 50},
      {"F", 50},
  };
  size_t selected_bar = 0;
  std::string status = std::string(port) + " " + std::to_string(baud_rate) +
                       " baud";

  while (keep_running) {
    std::string serial_text = serial.readAvailable();
    if (!serial_text.empty()) {
      appendSerialText(lines, partial_line, serial_text, 1000);
    }

    int key = getch();
    if (key == 27) {
      break;
    }
    if (key == KEY_LEFT && selected_bar > 0) {
      --selected_bar;
    } else if (key == KEY_RIGHT && selected_bar + 1 < bars.size()) {
      ++selected_bar;
    } else if (key == KEY_UP) {
      bars[selected_bar].value = std::min(100, bars[selected_bar].value + 1);
    } else if (key == KEY_DOWN) {
      bars[selected_bar].value = std::max(0, bars[selected_bar].value - 1);
    } else if (key == KEY_RESIZE) {
      clear();
    }

    erase();
    int footer_height = 1;
    int bars_height = std::min(10, std::max(0, LINES / 4));
    int messages_height = std::max(3, LINES - bars_height - footer_height);
    drawMessages(messages_height, lines, partial_line, status);
    drawBars(messages_height, bars_height, bars, selected_bar);
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
