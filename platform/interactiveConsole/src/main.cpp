#include "model/ColorPairs.h"
#include "model/Element.h"
#include "model/HardwareReportParser.h"
#include "model/OperationModes.h"
#include "model/PowerSupplyUnitElement.h"
#include "model/SteperMotorElement.h"
#include "gui/ConsoleWidget.h"
#include "gui/ElementsWidget.h"
#include "gui/InteractiveCommandsInputWidget.h"
#include "gui/KeyGuideWidget.h"
#include "io/SerialPort.h"

#include <algorithm>
#include <chrono>
#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <ncurses.h>
#include <string>
#include <vector>

static volatile sig_atomic_t keep_running = 1;
static const std::chrono::seconds RECONNECT_INTERVAL(2);
static const std::chrono::milliseconds HARDWARE_REPORT_TIMEOUT(800);
static const std::chrono::seconds PSU_POLL_INTERVAL(10);
static const int KEY_GUIDE_HEIGHT = 1;
static const int INPUT_HEIGHT = 1;
static const int ELEMENTS_MODE_CONSOLE_HEIGHT = 5;

static void handleSignal(int) {
  keep_running = 0;
}

// Splits raw serial text into complete lines, mirroring how ConsoleWidget
// parses its own scrollback, so the "hardware" command reply can be
// captured verbatim while it is still shown in the console.
static void appendRawLines(
    std::string &partial_line,
    std::vector<std::string> &out_lines,
    const std::string &text) {
  for (char character : text) {
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      out_lines.push_back(partial_line);
      partial_line.clear();
      continue;
    }
    if (character >= 32 || character == '\t') {
      partial_line += character;
    }
  }
}

// Parses an async "EVENT PSU=READY|LOST VMotor=<v>V" line, emitted by the
// firmware when the debounced power supply state flips. Returns false if
// the line does not match.
static bool parseEventPsuLine(
    const std::string &line, bool &out_available, int &out_millivolts) {
  static const std::string PREFIX = "EVENT PSU=";
  if (line.compare(0, PREFIX.size(), PREFIX) != 0) {
    return false;
  }
  const size_t space_pos = line.find(' ', PREFIX.size());
  if (space_pos == std::string::npos) {
    return false;
  }
  const std::string state = line.substr(PREFIX.size(), space_pos - PREFIX.size());
  static const std::string VOLTAGE_PREFIX = "VMotor=";
  const size_t voltage_pos = line.find(VOLTAGE_PREFIX, space_pos);
  if (voltage_pos == std::string::npos) {
    return false;
  }
  const double volts = atof(line.c_str() + voltage_pos + VOLTAGE_PREFIX.size());
  out_available = (state == "READY");
  out_millivolts = static_cast<int>(volts * 1000.0 + 0.5);
  return true;
}

// Routes one completed serial line to every element's applyStatusLine, so a
// "PSU,ON|OFF,<mV>" reply to ". <id>"/"get <id>" (or the equivalent state
// carried by an async "EVENT PSU=..." line) updates the matching widget.
// Elements ignore CSV lines that do not name their own type, so this is
// safe to call for arbitrary serial output.
static void routeIncomingStatusLine(
    const std::string &line, std::vector<std::unique_ptr<Element>> &elements) {
  bool event_available = false;
  int event_millivolts = 0;
  std::string status_line = line;
  if (parseEventPsuLine(line, event_available, event_millivolts)) {
    char buffer[32];
    snprintf(
        buffer,
        sizeof(buffer),
        "PSU,%s,%d",
        event_available ? "ON" : "OFF",
        event_millivolts);
    status_line = buffer;
  }
  for (auto &element : elements) {
    element->applyStatusLine(status_line);
  }
}

// Motors have no direct visibility into the PSU element's state, so after
// any update this pushes the combined "PSU present AND motorDriverEnabled
// flag" state from the PSU element into every motor element, which then
// hides its own test/move-by-steps widgets while disabled.
static void syncMotorDriverEnabledAcrossElements(
    std::vector<std::unique_ptr<Element>> &elements) {
  bool motors_actually_enabled = true;
  for (const auto &element : elements) {
    const auto *psu = dynamic_cast<const PowerSupplyUnitElement *>(element.get());
    if (psu != nullptr) {
      motors_actually_enabled = psu->motorsActuallyEnabled();
    }
  }
  for (auto &element : elements) {
    auto *motor = dynamic_cast<SteperMotorElement *>(element.get());
    if (motor != nullptr) {
      motor->setMotorDriverEnabled(motors_actually_enabled);
    }
  }
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
  init_pair(ERROR_TEXT_PAIR, COLOR_RED, -1);
  init_pair(SELECTED_ELEMENT_PAIR, COLOR_GREEN, -1);
  init_pair(UNSELECTED_ELEMENT_PAIR, COLOR_WHITE, -1);

  ConsoleWidget console;
  KeyGuideWidget key_guide;
  InteractiveCommandsInputWidget command_input;
  ElementsWidget elements_widget;

  OperationModes mode = CONSOLE_MODE;
  std::string status = std::string(port) + " " + std::to_string(baud_rate) +
                       " baud";
  bool connected = true;
  std::chrono::steady_clock::time_point next_reconnect_attempt;

  std::vector<std::unique_ptr<Element>> elements;
  // Keyboard focus model: focus is either the console input line, or one
  // of the Elements boxes. <Tab>/<Shift-Tab> move circularly through
  // console -> element 0 -> ... -> element N-1 -> console.
  bool console_focused = true;
  size_t focused_element_index = 0U;
  bool collecting_hardware_report = false;
  std::vector<std::string> hardware_report_lines;
  std::string hardware_report_partial_line;
  std::chrono::steady_clock::time_point hardware_report_deadline;
  std::string status_line_partial;
  std::chrono::steady_clock::time_point next_psu_poll =
      std::chrono::steady_clock::now();

  auto pollPsuStatus = [&]() {
    if (!connected) {
      return;
    }
    for (auto &element : elements) {
      if (dynamic_cast<PowerSupplyUnitElement *>(element.get()) != nullptr) {
        serial.writeText(". " + std::to_string(element->id()) + "\n");
      }
    }
  };

  auto requestHardwareReport = [&]() {
    if (!connected) {
      return;
    }
    serial.writeText("hardware\n");
    collecting_hardware_report = true;
    hardware_report_lines.clear();
    hardware_report_partial_line.clear();
    hardware_report_deadline =
        std::chrono::steady_clock::now() + HARDWARE_REPORT_TIMEOUT;
  };

  auto finishHardwareReport = [&]() {
    elements = HardwareReportParser::parse(hardware_report_lines);
    collecting_hardware_report = false;
    if (elements.empty()) {
      console_focused = true;
      focused_element_index = 0U;
    } else if (focused_element_index >= elements.size()) {
      focused_element_index = elements.size() - 1U;
    }
  };

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
        if (collecting_hardware_report) {
          appendRawLines(
              hardware_report_partial_line, hardware_report_lines, serial_text);
          if (!hardware_report_lines.empty() &&
              hardware_report_lines.back().find("power supply") !=
                  std::string::npos) {
            finishHardwareReport();
          }
        }
        std::vector<std::string> status_lines;
        appendRawLines(status_line_partial, status_lines, serial_text);
        for (const std::string &status_line : status_lines) {
          routeIncomingStatusLine(status_line, elements);
        }
        if (!status_lines.empty()) {
          syncMotorDriverEnabledAcrossElements(elements);
        }
      }
    } else if (std::chrono::steady_clock::now() >= next_reconnect_attempt) {
      if (!serial.open(port, baud_rate)) {
        next_reconnect_attempt =
            std::chrono::steady_clock::now() + RECONNECT_INTERVAL;
      } else {
        connected = true;
      }
    }

    if (connected && mode == ELEMENTS_MODE &&
        std::chrono::steady_clock::now() >= next_psu_poll) {
      pollPsuStatus();
      next_psu_poll = std::chrono::steady_clock::now() + PSU_POLL_INTERVAL;
    }

    const int available_height =
        std::max(0, LINES - KEY_GUIDE_HEIGHT - INPUT_HEIGHT);
    const int console_height = std::max(
        3,
        (mode == CONSOLE_MODE) ?
            available_height :
            std::min(ELEMENTS_MODE_CONSOLE_HEIGHT, available_height));
    const int elements_height = available_height - console_height;
    const int elements_top_row = 0;
    const int console_top_row = elements_height;
    const int visible_rows = std::max(1, console_height - 2);

    int key = getch();
    if (key == 27) {
      break;
    }

    if (key == KEY_RESIZE) {
      clear();
    } else if (key == KEY_PPAGE) {
      console.scrollPageUp(visible_rows);
    } else if (key == KEY_NPAGE) {
      console.scrollPageDown(visible_rows);
    } else if (key == KEY_F(1)) {
      mode = CONSOLE_MODE;
      console_focused = true;
      clear();
    } else if (key == KEY_F(2)) {
      mode = ELEMENTS_MODE;
      console_focused = true;
      clear();
      requestHardwareReport();
      next_psu_poll = std::chrono::steady_clock::now();
    } else if (mode == ELEMENTS_MODE && key == static_cast<int>('\t')) {
      if (console_focused) {
        if (!elements.empty()) {
          console_focused = false;
          focused_element_index = 0U;
        }
      } else if (focused_element_index + 1U < elements.size()) {
        ++focused_element_index;
      } else {
        console_focused = true;
      }
    } else if (mode == ELEMENTS_MODE && key == KEY_BTAB) {
      if (console_focused) {
        if (!elements.empty()) {
          console_focused = false;
          focused_element_index = elements.size() - 1U;
        }
      } else if (focused_element_index > 0U) {
        --focused_element_index;
      } else {
        console_focused = true;
      }
    } else if (console_focused) {
      if (key == KEY_DOWN) {
        command_input.navigateHistoryDown();
      } else if (key == KEY_UP) {
        command_input.navigateHistoryUp();
      } else if (command_input.handleKey(key)) {
        std::string message_to_send = command_input.takeCommand() + "\n";
        if (connected && !serial.writeText(message_to_send)) {
          console.appendSerialText("[serial write error]\n");
        }
      }
    } else if (!elements.empty()) {
      std::string command_to_send;
      if (elements[focused_element_index]->handleControlKey(
              key, command_to_send) &&
          !command_to_send.empty()) {
        if (connected && !serial.writeText(command_to_send + "\n")) {
          console.appendSerialText("[serial write error]\n");
        }
      }
    }

    if (collecting_hardware_report &&
        std::chrono::steady_clock::now() >= hardware_report_deadline) {
      finishHardwareReport();
    }

    erase();
    if (mode == ELEMENTS_MODE && elements_height > 0) {
      const size_t highlighted_index =
          console_focused ? elements.size() : focused_element_index;
      elements_widget.draw(
          elements_top_row, elements_height, elements, highlighted_index);
    }
    console.draw(console_top_row, console_height, status);
    key_guide.draw(
        console_top_row + console_height,
        mode,
        !console_focused && !elements.empty());
    command_input.draw(
        console_top_row + console_height + KEY_GUIDE_HEIGHT, console_focused);
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
