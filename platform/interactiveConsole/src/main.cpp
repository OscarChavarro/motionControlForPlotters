#include "model/ColorPairs.h"
#include "model/Device.h"
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
#include <deque>
#include <memory>
#include <ncurses.h>
#include <pthread.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>

static volatile sig_atomic_t keep_running = 1;
static const std::chrono::seconds RECONNECT_INTERVAL(2);
static const std::chrono::milliseconds HARDWARE_REPORT_TIMEOUT(800);
static const std::chrono::milliseconds DEVICE_RESPONSE_TIMEOUT(800);
static const std::chrono::milliseconds DEVICE_BOOT_TIMEOUT(1200);
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

static std::string lastWord(const std::string &text) {
  const size_t end = text.find_last_not_of(' ');
  if (end == std::string::npos) {
    return std::string();
  }
  const size_t start = text.find_last_of(' ', end);
  return text.substr(
      (start == std::string::npos) ? 0U : start + 1U,
      end - ((start == std::string::npos) ? 0U : start + 1U) + 1U);
}

struct ConnectedDevice {
  ConnectedDevice(
      std::string port_name,
      Device device_description,
      std::unique_ptr<SerialPort> serial_port)
      : port(std::move(port_name)),
        device(std::move(device_description)),
        serial(std::move(serial_port)) {}

  std::string port;
  Device device;
  std::unique_ptr<SerialPort> serial;
};

struct SerialEvent {
  SerialEvent(bool error_value, std::string text_value)
      : error(error_value), text(std::move(text_value)) {}

  bool error;
  std::string text;
};

struct DeviceControlState {
  pthread_mutex_t mutex;
  pthread_cond_t startup_cond;
  pthread_cond_t command_cond;
  std::vector<std::string> ports;
  int baud_rate;
  bool shutdown_requested;
  bool startup_complete;
  bool expected_found;
  bool connected;
  std::string active_port;
  std::string active_hardware_description;
  size_t device_connection_count;
  std::deque<std::string> commands;
  std::deque<SerialEvent> events;
};

static void initDeviceControlState(
    DeviceControlState &state,
    const std::vector<std::string> &ports,
    int baud_rate) {
  pthread_mutex_init(&state.mutex, nullptr);
  pthread_cond_init(&state.startup_cond, nullptr);
  pthread_cond_init(&state.command_cond, nullptr);
  state.ports = ports;
  state.baud_rate = baud_rate;
  state.shutdown_requested = false;
  state.startup_complete = false;
  state.expected_found = false;
  state.connected = false;
  state.active_port.clear();
  state.active_hardware_description.clear();
  state.device_connection_count = 0U;
  state.commands.clear();
  state.events.clear();
}

static void destroyDeviceControlState(DeviceControlState &state) {
  pthread_cond_destroy(&state.command_cond);
  pthread_cond_destroy(&state.startup_cond);
  pthread_mutex_destroy(&state.mutex);
}

static void addMillisecondsToTimespec(timespec &time, long milliseconds) {
  time.tv_nsec += (milliseconds % 1000L) * 1000000L;
  time.tv_sec += milliseconds / 1000L;
  if (time.tv_nsec >= 1000000000L) {
    ++time.tv_sec;
    time.tv_nsec -= 1000000000L;
  }
}

static void waitForCommandsOrTick(DeviceControlState &state) {
  timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  addMillisecondsToTimespec(deadline, 30L);

  pthread_mutex_lock(&state.mutex);
  if (!state.shutdown_requested && state.commands.empty()) {
    pthread_cond_timedwait(&state.command_cond, &state.mutex, &deadline);
  }
  pthread_mutex_unlock(&state.mutex);
}

static void publishStartupResult(
    DeviceControlState &state,
    bool expected_found,
    bool connected,
    const std::string &active_port,
    const std::string &active_hardware_description,
    size_t device_connection_count) {
  pthread_mutex_lock(&state.mutex);
  state.expected_found = expected_found;
  state.connected = connected;
  state.active_port = active_port;
  state.active_hardware_description = active_hardware_description;
  state.device_connection_count = device_connection_count;
  state.startup_complete = true;
  pthread_cond_broadcast(&state.startup_cond);
  pthread_mutex_unlock(&state.mutex);
}

static void publishConnected(DeviceControlState &state, bool connected) {
  pthread_mutex_lock(&state.mutex);
  state.connected = connected;
  pthread_mutex_unlock(&state.mutex);
}

static void publishSerialText(DeviceControlState &state, const std::string &text) {
  pthread_mutex_lock(&state.mutex);
  state.events.emplace_back(false, text);
  pthread_mutex_unlock(&state.mutex);
}

static void publishSerialError(DeviceControlState &state, const std::string &text) {
  pthread_mutex_lock(&state.mutex);
  state.events.emplace_back(true, text);
  pthread_mutex_unlock(&state.mutex);
}

static bool takeShutdownRequested(DeviceControlState &state) {
  pthread_mutex_lock(&state.mutex);
  const bool shutdown_requested = state.shutdown_requested;
  pthread_mutex_unlock(&state.mutex);
  return shutdown_requested;
}

static std::deque<std::string> takePendingCommands(DeviceControlState &state) {
  pthread_mutex_lock(&state.mutex);
  std::deque<std::string> commands;
  commands.swap(state.commands);
  pthread_mutex_unlock(&state.mutex);
  return commands;
}

static void enqueueDeviceCommand(
    DeviceControlState &state, const std::string &command) {
  pthread_mutex_lock(&state.mutex);
  if (!state.shutdown_requested) {
    state.commands.push_back(command);
    pthread_cond_signal(&state.command_cond);
  }
  pthread_mutex_unlock(&state.mutex);
}

static void requestDeviceShutdown(DeviceControlState &state) {
  pthread_mutex_lock(&state.mutex);
  state.shutdown_requested = true;
  pthread_cond_broadcast(&state.command_cond);
  pthread_mutex_unlock(&state.mutex);
}

static std::deque<SerialEvent> takeSerialEvents(
    DeviceControlState &state,
    bool &connected,
    std::string &active_port,
    std::string &active_hardware_description,
    size_t &device_connection_count) {
  pthread_mutex_lock(&state.mutex);
  std::deque<SerialEvent> events;
  events.swap(state.events);
  connected = state.connected;
  active_port = state.active_port;
  active_hardware_description = state.active_hardware_description;
  device_connection_count = state.device_connection_count;
  pthread_mutex_unlock(&state.mutex);
  return events;
}

static std::unique_ptr<ConnectedDevice> connectToDevice(
    const char *port, int baud_rate) {
  std::unique_ptr<SerialPort> serial(new SerialPort(port, baud_rate));
  if (!serial->isOpen()) {
    return nullptr;
  }

  // Opening classic Arduino USB serial ports can reset their firmware. Drain
  // startup output before issuing the protocol probe.
  const std::chrono::steady_clock::time_point boot_deadline =
      std::chrono::steady_clock::now() + DEVICE_BOOT_TIMEOUT;
  while (std::chrono::steady_clock::now() < boot_deadline) {
    bool connection_lost = false;
    serial->readAvailable(connection_lost);
    if (connection_lost) {
      return nullptr;
    }
    usleep(20U * 1000U);
  }

  if (!serial->writeText("device\n")) {
    return nullptr;
  }

  std::string partial_line;
  std::vector<std::string> lines;
  const std::chrono::steady_clock::time_point response_deadline =
      std::chrono::steady_clock::now() + DEVICE_RESPONSE_TIMEOUT;
  while (std::chrono::steady_clock::now() < response_deadline) {
    bool connection_lost = false;
    const std::string serial_text = serial->readAvailable(connection_lost);
    if (connection_lost) {
      return nullptr;
    }
    if (!serial_text.empty()) {
      appendRawLines(partial_line, lines, serial_text);
      for (const std::string &line : lines) {
        Device device("", "");
        if (Device::parseDescription(line, device)) {
          return std::unique_ptr<ConnectedDevice>(new ConnectedDevice(
              port, std::move(device), std::move(serial)));
        }
      }
      lines.clear();
    }
    usleep(20U * 1000U);
  }

  return nullptr;
}

static void *runDeviceControlThread(void *argument) {
  DeviceControlState *state = static_cast<DeviceControlState *>(argument);

  std::vector<std::unique_ptr<ConnectedDevice>> connected_devices;
  for (const std::string &port : state->ports) {
    if (takeShutdownRequested(*state)) {
      publishStartupResult(*state, false, false, std::string(), std::string(), 0U);
      return nullptr;
    }

    std::unique_ptr<ConnectedDevice> device = connectToDevice(
        port.c_str(), state->baud_rate);
    if (device != nullptr) {
      connected_devices.push_back(std::move(device));
    }
  }

  const std::string expected_hardware = "arduino ATmega2650";
  const std::string expected_role = "XY steper motor controller";
  auto active_device = std::find_if(
      connected_devices.begin(),
      connected_devices.end(),
      [&expected_hardware, &expected_role](
          const std::unique_ptr<ConnectedDevice> &device) {
        return device->device.hardwareDescription() == expected_hardware &&
               device->device.role() == expected_role;
      });

  if (active_device == connected_devices.end()) {
    publishStartupResult(*state, false, false, std::string(), std::string(),
                         connected_devices.size());
    return nullptr;
  }

  ConnectedDevice &selected_device = **active_device;
  SerialPort &serial = *selected_device.serial;
  bool connected = serial.isOpen();
  std::chrono::steady_clock::time_point next_reconnect_attempt =
      std::chrono::steady_clock::now();

  publishStartupResult(
      *state,
      true,
      connected,
      selected_device.port,
      selected_device.device.hardwareDescription(),
      connected_devices.size());

  while (!takeShutdownRequested(*state)) {
    std::deque<std::string> commands = takePendingCommands(*state);
    while (!commands.empty()) {
      if (connected && !serial.writeText(commands.front())) {
        publishSerialError(*state, "[serial write error]");
      }
      commands.pop_front();
    }

    if (connected) {
      bool connection_lost = false;
      const std::string serial_text = serial.readAvailable(connection_lost);
      if (connection_lost) {
        publishSerialError(*state, "[serial error: connection lost, retrying]");
        connected = false;
        publishConnected(*state, false);
        next_reconnect_attempt =
            std::chrono::steady_clock::now() + RECONNECT_INTERVAL;
      } else if (!serial_text.empty()) {
        publishSerialText(*state, serial_text);
      }
    } else if (std::chrono::steady_clock::now() >= next_reconnect_attempt) {
      if (!serial.open(selected_device.port.c_str(), state->baud_rate)) {
        next_reconnect_attempt =
            std::chrono::steady_clock::now() + RECONNECT_INTERVAL;
      } else {
        connected = true;
        publishConnected(*state, true);
      }
    }

    waitForCommandsOrTick(*state);
  }

  return nullptr;
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

static int runConsole(const std::vector<std::string> &ports, int baud_rate) {
  setlocale(LC_ALL, "");

  DeviceControlState device_control;
  initDeviceControlState(device_control, ports, baud_rate);

  pthread_t device_thread;
  if (pthread_create(
          &device_thread, nullptr, runDeviceControlThread, &device_control) != 0) {
    destroyDeviceControlState(device_control);
    fprintf(stderr, "Could not start device control thread.\n");
    return 1;
  }

  pthread_mutex_lock(&device_control.mutex);
  while (!device_control.startup_complete) {
    pthread_cond_wait(&device_control.startup_cond, &device_control.mutex);
  }
  const bool expected_found = device_control.expected_found;
  pthread_mutex_unlock(&device_control.mutex);

  if (!expected_found) {
    requestDeviceShutdown(device_control);
    pthread_join(device_thread, nullptr);
    destroyDeviceControlState(device_control);
    fprintf(
        stderr,
        "Expected Device and Role were not found on any serial port.\n");
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
  bool connected = true;
  std::string active_port;
  std::string active_hardware_description;
  std::string element_title_prefix;
  size_t device_connection_count = 0U;

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
        enqueueDeviceCommand(
            device_control, ". " + std::to_string(element->id()) + "\n");
      }
    }
  };

  auto requestHardwareReport = [&]() {
    if (!connected) {
      return;
    }
    enqueueDeviceCommand(device_control, "hardware\n");
    collecting_hardware_report = true;
    hardware_report_lines.clear();
    hardware_report_partial_line.clear();
    hardware_report_deadline =
        std::chrono::steady_clock::now() + HARDWARE_REPORT_TIMEOUT;
  };

  auto finishHardwareReport = [&]() {
    elements = HardwareReportParser::parse(
        hardware_report_lines, element_title_prefix);
    collecting_hardware_report = false;
    if (elements.empty()) {
      console_focused = true;
      focused_element_index = 0U;
    } else if (focused_element_index >= elements.size()) {
      focused_element_index = elements.size() - 1U;
    }
  };

  while (keep_running) {
    std::deque<SerialEvent> serial_events = takeSerialEvents(
        device_control,
        connected,
        active_port,
        active_hardware_description,
        device_connection_count);
    element_title_prefix = lastWord(active_hardware_description);
    while (!serial_events.empty()) {
      const SerialEvent event = serial_events.front();
      serial_events.pop_front();
      if (event.error) {
        console.appendErrorLine(event.text);
        continue;
      }

      console.appendSerialText(event.text);
      if (collecting_hardware_report) {
        appendRawLines(
            hardware_report_partial_line, hardware_report_lines, event.text);
        if (!hardware_report_lines.empty() &&
            hardware_report_lines.back().find("power supply") !=
                std::string::npos) {
          finishHardwareReport();
        }
      }
      std::vector<std::string> status_lines;
      appendRawLines(status_line_partial, status_lines, event.text);
      for (const std::string &status_line : status_lines) {
        routeIncomingStatusLine(status_line, elements);
      }
      if (!status_lines.empty()) {
        syncMotorDriverEnabledAcrossElements(elements);
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
        if (connected) {
          enqueueDeviceCommand(device_control, message_to_send);
        } else {
          console.appendErrorLine("[serial write error]");
        }
      }
    } else if (!elements.empty()) {
      std::string command_to_send;
      if (elements[focused_element_index]->handleControlKey(
              key, command_to_send) &&
          !command_to_send.empty()) {
        if (connected) {
          enqueueDeviceCommand(device_control, command_to_send + "\n");
        } else {
          console.appendErrorLine("[serial write error]");
        }
      }
    }

    if (collecting_hardware_report &&
        std::chrono::steady_clock::now() >= hardware_report_deadline) {
      finishHardwareReport();
    }

    erase();
    const std::string status = active_port + " " +
                               std::to_string(baud_rate) + " baud (" +
                               std::to_string(device_connection_count) +
                               " device connections)" +
                               (connected ? "" : " disconnected");
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

  requestDeviceShutdown(device_control);
  pthread_join(device_thread, nullptr);
  destroyDeviceControlState(device_control);
  endwin();
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <baud-rate> [serial-port...]\n", argv[0]);
    return 2;
  }

  char *end = nullptr;
  long baud_rate = strtol(argv[1], &end, 10);
  if (end == argv[1] || *end != '\0' || baud_rate <= 0) {
    fprintf(stderr, "Invalid baud rate: %s\n", argv[1]);
    return 2;
  }

  std::vector<std::string> ports;
  for (int index = 2; index < argc; ++index) {
    ports.emplace_back(argv[index]);
  }

  signal(SIGINT, handleSignal);
  signal(SIGTERM, handleSignal);

  return runConsole(ports, static_cast<int>(baud_rate));
}
