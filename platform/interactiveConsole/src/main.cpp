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
#include "io/BlePort.h"
#include "io/SerialPort.h"

#include <algorithm>
#include <chrono>
#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <glob.h>
#include <map>
#include <memory>
#include <ncurses.h>
#include <set>
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
static const std::chrono::milliseconds BLE_SCAN_TIMEOUT(5000);
static const std::chrono::seconds PSU_POLL_INTERVAL(10);
static const std::chrono::seconds SERIAL_PORT_SCAN_INTERVAL(10);
static const std::chrono::seconds NO_SERIAL_PORT_SCAN_INTERVAL(5);
static const std::chrono::seconds BLE_SCAN_INTERVAL(10);
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
      int connection_id_value,
      std::string port_name,
      Device device_description,
      std::unique_ptr<SerialPort> serial_port)
      : connection_id(connection_id_value),
        port(std::move(port_name)),
        device(std::move(device_description)),
        serial(std::move(serial_port)) {
  }

  ConnectedDevice(
      int connection_id_value,
      std::string port_name,
      Device device_description,
      std::unique_ptr<BlePort> ble_port)
      : connection_id(connection_id_value),
        port(std::move(port_name)),
        device(std::move(device_description)),
        ble(std::move(ble_port)) {
  }

  int connection_id;
  std::string port;
  Device device;
  std::unique_ptr<SerialPort> serial;
  std::unique_ptr<BlePort> ble;

  bool isOpen() const {
    if (serial != nullptr) {
      return serial->isOpen();
    }
    return ble != nullptr && ble->isOpen();
  }

  bool writeText(const std::string &text) {
    if (serial != nullptr) {
      return serial->writeText(text);
    }
    return ble != nullptr && ble->writeText(text);
  }

  std::string readAvailable(bool &connection_lost) {
    if (serial != nullptr) {
      return serial->readAvailable(connection_lost);
    }
    if (ble != nullptr) {
      return ble->readAvailable(connection_lost);
    }
    connection_lost = true;
    return std::string();
  }
};

struct SerialEvent {
  SerialEvent(int connection_id_value, bool error_value, std::string text_value)
      : connection_id(connection_id_value),
        error(error_value),
        text(std::move(text_value)) {}

  int connection_id;
  bool error;
  std::string text;
};

struct DeviceCommand {
  DeviceCommand(int connection_id_value, std::string text_value)
      : connection_id(connection_id_value), text(std::move(text_value)) {
  }

  int connection_id;
  std::string text;
};

struct HardwareReportCollection {
  std::vector<std::string> lines;
  std::string partial_line;
};

struct DeviceConnectionInfo {
  int connection_id;
  std::string port;
  std::string hardware_description;
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
  std::vector<DeviceConnectionInfo> device_infos;
  std::deque<DeviceCommand> commands;
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
  state.device_infos.clear();
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
    size_t device_connection_count,
    const std::vector<DeviceConnectionInfo> &device_infos) {
  pthread_mutex_lock(&state.mutex);
  state.expected_found = expected_found;
  state.connected = connected;
  state.active_port = active_port;
  state.active_hardware_description = active_hardware_description;
  state.device_connection_count = device_connection_count;
  state.device_infos = device_infos;
  state.startup_complete = true;
  pthread_cond_broadcast(&state.startup_cond);
  pthread_mutex_unlock(&state.mutex);
}

static void publishConnected(DeviceControlState &state, bool connected) {
  pthread_mutex_lock(&state.mutex);
  state.connected = connected;
  pthread_mutex_unlock(&state.mutex);
}

static void publishSerialText(
    DeviceControlState &state,
    int connection_id,
    const std::string &text) {
  pthread_mutex_lock(&state.mutex);
  state.events.emplace_back(connection_id, false, text);
  pthread_mutex_unlock(&state.mutex);
}

static void publishSerialError(
    DeviceControlState &state,
    int connection_id,
    const std::string &text) {
  pthread_mutex_lock(&state.mutex);
  state.events.emplace_back(connection_id, true, text);
  pthread_mutex_unlock(&state.mutex);
}

static bool takeShutdownRequested(DeviceControlState &state) {
  pthread_mutex_lock(&state.mutex);
  const bool shutdown_requested = state.shutdown_requested;
  pthread_mutex_unlock(&state.mutex);
  return shutdown_requested;
}

static std::deque<DeviceCommand> takePendingCommands(DeviceControlState &state) {
  pthread_mutex_lock(&state.mutex);
  std::deque<DeviceCommand> commands;
  commands.swap(state.commands);
  pthread_mutex_unlock(&state.mutex);
  return commands;
}

static void enqueueDeviceCommand(
    DeviceControlState &state, int connection_id, const std::string &command) {
  pthread_mutex_lock(&state.mutex);
  if (!state.shutdown_requested) {
    state.commands.emplace_back(connection_id, command);
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
    size_t &device_connection_count,
    std::vector<DeviceConnectionInfo> &device_infos) {
  pthread_mutex_lock(&state.mutex);
  std::deque<SerialEvent> events;
  events.swap(state.events);
  connected = state.connected;
  active_port = state.active_port;
  active_hardware_description = state.active_hardware_description;
  device_connection_count = state.device_connection_count;
  device_infos = state.device_infos;
  pthread_mutex_unlock(&state.mutex);
  return events;
}

static std::unique_ptr<ConnectedDevice> connectToDevice(
    int connection_id, const char *port, int baud_rate) {
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
              connection_id, port, std::move(device), std::move(serial)));
        }
      }
      lines.clear();
    }
    usleep(20U * 1000U);
  }

  return nullptr;
}

static void appendGlobMatches(
    const char *pattern,
    std::set<std::string> &ports) {
  glob_t matches;
  memset(&matches, 0, sizeof(matches));
  if (glob(pattern, 0, nullptr, &matches) == 0) {
    for (size_t index = 0; index < matches.gl_pathc; ++index) {
      ports.insert(matches.gl_pathv[index]);
    }
  }
  globfree(&matches);
}

static std::vector<std::string> discoverSerialPorts() {
  std::set<std::string> ports;
#ifdef __APPLE__
  appendGlobMatches("/dev/cu.usbmodem*", ports);
  appendGlobMatches("/dev/cu.usbserial*", ports);
#else
  appendGlobMatches("/dev/ttyACM*", ports);
  appendGlobMatches("/dev/ttyUSB*", ports);
#endif
  return std::vector<std::string>(ports.begin(), ports.end());
}

static std::vector<DeviceConnectionInfo> buildDeviceInfos(
    const std::vector<std::unique_ptr<ConnectedDevice>> &connected_devices) {
  std::vector<DeviceConnectionInfo> infos;
  for (const std::unique_ptr<ConnectedDevice> &device : connected_devices) {
    infos.push_back({
        device->connection_id,
        device->port,
        device->device.hardwareDescription(),
    });
  }
  return infos;
}

static void publishConnectionSnapshot(
    DeviceControlState &state,
    const std::vector<std::unique_ptr<ConnectedDevice>> &connected_devices) {
  bool connected = false;
  std::string active_ports;
  std::string active_hardware;
  for (const std::unique_ptr<ConnectedDevice> &device : connected_devices) {
    if (device->isOpen()) {
      connected = true;
    }
    if (!active_ports.empty()) {
      active_ports += ", ";
    }
    active_ports += device->port;
    if (!active_hardware.empty()) {
      active_hardware += ", ";
    }
    active_hardware += device->device.hardwareDescription();
  }

  publishStartupResult(
      state,
      true,
      connected,
      active_ports,
      active_hardware,
      connected_devices.size(),
      buildDeviceInfos(connected_devices));
}

static ConnectedDevice *findDeviceByPort(
    std::vector<std::unique_ptr<ConnectedDevice>> &connected_devices,
    const std::string &port) {
  for (std::unique_ptr<ConnectedDevice> &device : connected_devices) {
    if (device->port == port) {
      return device.get();
    }
  }
  return nullptr;
}

static bool eraseDeviceByConnectionId(
    std::vector<std::unique_ptr<ConnectedDevice>> &connected_devices,
    int connection_id) {
  const size_t old_size = connected_devices.size();
  connected_devices.erase(
      std::remove_if(
          connected_devices.begin(),
          connected_devices.end(),
          [connection_id](const std::unique_ptr<ConnectedDevice> &device) {
            return device->connection_id == connection_id;
          }),
      connected_devices.end());
  return connected_devices.size() != old_size;
}

static void scanSerialPorts(
    DeviceControlState &state,
    std::vector<std::unique_ptr<ConnectedDevice>> &connected_devices,
    int &next_connection_id) {
  std::set<std::string> candidate_ports(state.ports.begin(), state.ports.end());
  const std::vector<std::string> discovered_ports = discoverSerialPorts();
  candidate_ports.insert(discovered_ports.begin(), discovered_ports.end());

  bool changed = false;
  for (const std::string &port : candidate_ports) {
    ConnectedDevice *existing = findDeviceByPort(connected_devices, port);
    if (existing != nullptr && existing->isOpen()) {
      continue;
    }

    const int connection_id = existing != nullptr ?
        existing->connection_id :
        next_connection_id;
    std::unique_ptr<ConnectedDevice> device =
        connectToDevice(connection_id, port.c_str(), state.baud_rate);
    if (device == nullptr) {
      continue;
    }

    if (existing != nullptr) {
      for (std::unique_ptr<ConnectedDevice> &stored : connected_devices) {
        if (stored.get() == existing) {
          stored = std::move(device);
          break;
        }
      }
    } else {
      connected_devices.push_back(std::move(device));
      ++next_connection_id;
    }
    changed = true;
  }

  if (changed) {
    publishConnectionSnapshot(state, connected_devices);
  }
}

static std::unique_ptr<ConnectedDevice> connectToBleDevice(int connection_id) {
  std::unique_ptr<BlePort> ble = BlePort::scanAndConnect(BLE_SCAN_TIMEOUT);
  if (ble == nullptr || !ble->isOpen()) {
    return nullptr;
  }

  Device device("esp32 ESP-WROOM-32", "servo BLE controller");
  std::string name = ble->name();
  if (name.empty()) {
    name = "BLE servo";
  }
  return std::unique_ptr<ConnectedDevice>(new ConnectedDevice(
      connection_id, name, std::move(device), std::move(ble)));
}

static bool hasBleDevice(
    const std::vector<std::unique_ptr<ConnectedDevice>> &connected_devices) {
  for (const std::unique_ptr<ConnectedDevice> &device : connected_devices) {
    if (device->ble != nullptr) {
      return true;
    }
  }
  return false;
}

static bool hasSerialDevice(
    const std::vector<std::unique_ptr<ConnectedDevice>> &connected_devices) {
  for (const std::unique_ptr<ConnectedDevice> &device : connected_devices) {
    if (device->serial != nullptr) {
      return true;
    }
  }
  return false;
}

static std::chrono::seconds serialPortScanInterval(
    const std::vector<std::unique_ptr<ConnectedDevice>> &connected_devices) {
  return hasSerialDevice(connected_devices) ?
      SERIAL_PORT_SCAN_INTERVAL :
      NO_SERIAL_PORT_SCAN_INTERVAL;
}

static void scanBleDevices(
    DeviceControlState &state,
    std::vector<std::unique_ptr<ConnectedDevice>> &connected_devices,
    int &next_connection_id) {
  if (hasBleDevice(connected_devices)) {
    return;
  }

  std::unique_ptr<ConnectedDevice> ble_device =
      connectToBleDevice(next_connection_id);
  if (ble_device == nullptr) {
    return;
  }

  connected_devices.push_back(std::move(ble_device));
  ++next_connection_id;
  publishConnectionSnapshot(state, connected_devices);
}

static void *runDeviceControlThread(void *argument) {
  DeviceControlState *state = static_cast<DeviceControlState *>(argument);

  std::vector<std::unique_ptr<ConnectedDevice>> connected_devices;
  int next_connection_id = 0;
  std::set<std::string> startup_ports(state->ports.begin(), state->ports.end());
  const std::vector<std::string> discovered_startup_ports = discoverSerialPorts();
  startup_ports.insert(
      discovered_startup_ports.begin(), discovered_startup_ports.end());

  for (const std::string &port : startup_ports) {
    if (takeShutdownRequested(*state)) {
      publishStartupResult(
          *state,
          false,
          false,
          std::string(),
          std::string(),
          0U,
          std::vector<DeviceConnectionInfo>());
      return nullptr;
    }

    std::unique_ptr<ConnectedDevice> device = connectToDevice(
        next_connection_id, port.c_str(), state->baud_rate);
    if (device != nullptr) {
      connected_devices.push_back(std::move(device));
      ++next_connection_id;
    }
  }

  std::unique_ptr<ConnectedDevice> ble_device =
      connectToBleDevice(next_connection_id);
  if (ble_device != nullptr) {
    connected_devices.push_back(std::move(ble_device));
    ++next_connection_id;
  }

  publishConnectionSnapshot(*state, connected_devices);
  bool connected = false;
  for (const std::unique_ptr<ConnectedDevice> &device : connected_devices) {
    if (device->isOpen()) {
      connected = true;
    }
  }

  std::chrono::steady_clock::time_point next_serial_port_scan =
      std::chrono::steady_clock::now() +
      serialPortScanInterval(connected_devices);
  std::chrono::steady_clock::time_point next_ble_scan =
      std::chrono::steady_clock::now() + BLE_SCAN_INTERVAL;

  while (!takeShutdownRequested(*state)) {
    std::deque<DeviceCommand> commands = takePendingCommands(*state);
    while (!commands.empty()) {
      const DeviceCommand command = commands.front();
      for (std::unique_ptr<ConnectedDevice> &device : connected_devices) {
        if (command.connection_id >= 0 &&
            device->connection_id != command.connection_id) {
          continue;
        }
        if (device->isOpen() && !device->writeText(command.text)) {
          publishSerialError(
              *state, device->connection_id, "[device write error]");
        }
      }
      commands.pop_front();
    }

    if (std::chrono::steady_clock::now() >= next_serial_port_scan) {
      scanSerialPorts(*state, connected_devices, next_connection_id);
      next_serial_port_scan =
          std::chrono::steady_clock::now() +
          serialPortScanInterval(connected_devices);
    }
    if (std::chrono::steady_clock::now() >= next_ble_scan) {
      scanBleDevices(*state, connected_devices, next_connection_id);
      next_ble_scan = std::chrono::steady_clock::now() + BLE_SCAN_INTERVAL;
    }

    bool any_connected = false;
    std::vector<int> lost_connection_ids;
    for (std::unique_ptr<ConnectedDevice> &device : connected_devices) {
      if (!device->isOpen()) {
        if (device->ble != nullptr) {
          publishSerialError(
              *state,
              device->connection_id,
              "[device error: connection lost]");
          lost_connection_ids.push_back(device->connection_id);
        }
        continue;
      }
      any_connected = true;
      bool connection_lost = false;
      const std::string serial_text = device->readAvailable(connection_lost);
      if (connection_lost) {
        publishSerialError(
            *state,
            device->connection_id,
            "[device error: connection lost]");
        lost_connection_ids.push_back(device->connection_id);
      } else if (!serial_text.empty()) {
        publishSerialText(*state, device->connection_id, serial_text);
      }
    }
    if (!lost_connection_ids.empty()) {
      for (int connection_id : lost_connection_ids) {
        eraseDeviceByConnectionId(connected_devices, connection_id);
      }
      publishConnectionSnapshot(*state, connected_devices);
      next_serial_port_scan =
          std::chrono::steady_clock::now() +
          serialPortScanInterval(connected_devices);
    }
    if (any_connected != connected) {
      connected = any_connected;
      publishConnected(*state, connected);
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

static std::string titlePrefixForConnection(
    const std::vector<DeviceConnectionInfo> &device_infos,
    int connection_id) {
  for (const DeviceConnectionInfo &info : device_infos) {
    if (info.connection_id == connection_id) {
      return lastWord(info.hardware_description);
    }
  }
  return std::string();
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

  std::map<int, ConsoleWidget> consoles;
  std::vector<DeviceConnectionInfo> device_infos;
  int selected_console_connection_id = -1;
  KeyGuideWidget key_guide;
  InteractiveCommandsInputWidget command_input;
  ElementsWidget elements_widget;

  OperationModes mode = CONSOLE_MODE;
  bool connected = true;
  std::string active_port;
  std::string active_hardware_description;
  size_t device_connection_count = 0U;

  std::vector<std::unique_ptr<Element>> elements;
  std::set<int> known_connection_ids;
  // Keyboard focus model: focus is either the console input line, or one
  // of the Elements boxes. <Tab>/<Shift-Tab> move circularly through
  // console -> element 0 -> ... -> element N-1 -> console.
  bool console_focused = true;
  size_t focused_element_index = 0U;
  bool collecting_hardware_report = false;
  std::map<int, HardwareReportCollection> hardware_reports;
  std::chrono::steady_clock::time_point hardware_report_deadline;
  std::map<int, std::string> status_line_partials;
  std::chrono::steady_clock::time_point next_psu_poll =
      std::chrono::steady_clock::now();

  auto pollPsuStatus = [&]() {
    if (!connected) {
      return;
    }
    for (auto &element : elements) {
      if (dynamic_cast<PowerSupplyUnitElement *>(element.get()) != nullptr) {
        enqueueDeviceCommand(
            device_control,
            element->connectionId(),
            ". " + std::to_string(element->id()) + "\n");
      }
    }
  };

  auto requestHardwareReport = [&]() {
    if (!connected) {
      return;
    }
    enqueueDeviceCommand(device_control, -1, "hardware\n");
    collecting_hardware_report = true;
    hardware_reports.clear();
    hardware_report_deadline =
        std::chrono::steady_clock::now() + HARDWARE_REPORT_TIMEOUT;
  };

  auto finishHardwareReport = [&]() {
    std::vector<std::unique_ptr<Element>> parsed_elements;
    std::set<int> active_connection_ids;
    for (const DeviceConnectionInfo &info : device_infos) {
      active_connection_ids.insert(info.connection_id);
    }
    for (const std::pair<const int, HardwareReportCollection> &report :
         hardware_reports) {
      if (active_connection_ids.find(report.first) ==
          active_connection_ids.end()) {
        continue;
      }
      std::vector<std::unique_ptr<Element>> device_elements =
          HardwareReportParser::parse(
              report.second.lines,
              titlePrefixForConnection(device_infos, report.first),
              report.first);
      for (std::unique_ptr<Element> &element : device_elements) {
        parsed_elements.push_back(std::move(element));
      }
    }
    elements = std::move(parsed_elements);
    collecting_hardware_report = false;
    if (elements.empty()) {
      console_focused = true;
      focused_element_index = 0U;
    } else if (focused_element_index >= elements.size()) {
      focused_element_index = elements.size() - 1U;
    }
    for (const std::unique_ptr<Element> &element : elements) {
      enqueueDeviceCommand(
          device_control,
          element->connectionId(),
          ". " + std::to_string(element->id()) + "\n");
    }
  };

  while (keep_running) {
    std::deque<SerialEvent> serial_events = takeSerialEvents(
        device_control,
        connected,
        active_port,
        active_hardware_description,
        device_connection_count,
        device_infos);
    std::set<int> active_connection_ids;
    bool found_new_connection = false;
    for (const DeviceConnectionInfo &info : device_infos) {
      active_connection_ids.insert(info.connection_id);
      if (known_connection_ids.find(info.connection_id) ==
          known_connection_ids.end()) {
        found_new_connection = true;
      }
    }
    known_connection_ids = active_connection_ids;
    if (!active_connection_ids.empty()) {
      elements.erase(
          std::remove_if(
              elements.begin(),
              elements.end(),
              [&active_connection_ids](
                  const std::unique_ptr<Element> &element) {
                return active_connection_ids.find(element->connectionId()) ==
                    active_connection_ids.end();
              }),
          elements.end());
      if (focused_element_index >= elements.size() && !elements.empty()) {
        focused_element_index = elements.size() - 1U;
      }
      if (elements.empty()) {
        focused_element_index = 0U;
        console_focused = true;
      }
    } else {
      elements.clear();
      focused_element_index = 0U;
      console_focused = true;
    }
    for (const DeviceConnectionInfo &info : device_infos) {
      if (consoles.find(info.connection_id) == consoles.end()) {
        consoles.emplace(info.connection_id, ConsoleWidget());
      }
    }
    if (selected_console_connection_id < 0 && !device_infos.empty()) {
      selected_console_connection_id = device_infos.front().connection_id;
    }
    if (selected_console_connection_id < 0) {
      consoles.emplace(-1, ConsoleWidget());
      selected_console_connection_id = -1;
    }
    if (!active_connection_ids.empty() &&
        active_connection_ids.find(selected_console_connection_id) ==
            active_connection_ids.end()) {
      selected_console_connection_id = device_infos.front().connection_id;
    }
    ConsoleWidget &selected_console = consoles[selected_console_connection_id];
    while (!serial_events.empty()) {
      const SerialEvent event = serial_events.front();
      serial_events.pop_front();
      ConsoleWidget &event_console = consoles[event.connection_id];
      if (event.error) {
        event_console.appendErrorLine(event.text);
        continue;
      }

      event_console.appendSerialText(event.text);
      if (collecting_hardware_report) {
        HardwareReportCollection &report = hardware_reports[event.connection_id];
        appendRawLines(
            report.partial_line, report.lines, event.text);
      }
      std::vector<std::string> status_lines;
      appendRawLines(
          status_line_partials[event.connection_id], status_lines, event.text);
      for (const std::string &status_line : status_lines) {
        routeIncomingStatusLine(status_line, elements);
      }
      if (!status_lines.empty()) {
        syncMotorDriverEnabledAcrossElements(elements);
      }
    }

    if (found_new_connection && mode == ELEMENTS_MODE) {
      requestHardwareReport();
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
      selected_console.scrollPageUp(visible_rows);
    } else if (key == KEY_NPAGE) {
      selected_console.scrollPageDown(visible_rows);
    } else if (key == KEY_F(1)) {
      mode = CONSOLE_MODE;
      console_focused = true;
      if (!device_infos.empty()) {
        size_t current_index = 0U;
        for (size_t index = 0U; index < device_infos.size(); ++index) {
          if (device_infos[index].connection_id == selected_console_connection_id) {
            current_index = index;
            break;
          }
        }
        selected_console_connection_id =
            device_infos[(current_index + 1U) % device_infos.size()].connection_id;
      }
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
          enqueueDeviceCommand(device_control, -1, message_to_send);
        } else {
          selected_console.appendErrorLine("[device write error]");
        }
      }
    } else if (!elements.empty()) {
      std::string command_to_send;
      if (elements[focused_element_index]->handleControlKey(
              key, command_to_send) &&
          !command_to_send.empty()) {
        if (connected) {
          enqueueDeviceCommand(
              device_control,
              elements[focused_element_index]->connectionId(),
              command_to_send + "\n");
          enqueueDeviceCommand(
              device_control,
              elements[focused_element_index]->connectionId(),
              ". " + std::to_string(elements[focused_element_index]->id()) +
                  "\n");
        } else {
          selected_console.appendErrorLine("[device write error]");
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
    std::string selected_console_status = status;
    for (const DeviceConnectionInfo &info : device_infos) {
      if (info.connection_id == selected_console_connection_id) {
        selected_console_status = info.port + " (" +
            info.hardware_description + ") [" +
            std::to_string(device_connection_count) + " device connections]" +
            (connected ? "" : " disconnected");
        break;
      }
    }
    if (mode == ELEMENTS_MODE && elements_height > 0) {
      const size_t highlighted_index =
          console_focused ? elements.size() : focused_element_index;
      elements_widget.draw(
          elements_top_row, elements_height, elements, highlighted_index);
    }
    selected_console.draw(console_top_row, console_height, selected_console_status);
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
