#include "HardwareReportParser.h"

#include "PowerSupplyUnitElement.h"
#include "ServoElement.h"
#include "SteperMotorElement.h"

#include <cctype>
#include <cstring>

namespace {

// Firmware lines look like "[0] D3, D2: stepper motor 0 (direction, step),
// Artillery D42HSA5401-23B driven by A4988" or "[1] A0: external power
// supply detector input". Returns the parsed id and the text after "] ",
// or false if the line has no [id] tag (e.g. the report header).
bool tryParseElementId(const std::string &line, int &out_id, std::string &out_rest) {
  if (line.empty() || line[0] != '[') {
    return false;
  }
  const size_t close_bracket = line.find(']');
  if (close_bracket == std::string::npos || close_bracket == 1U) {
    return false;
  }

  const std::string id_text = line.substr(1, close_bracket - 1);
  for (char character : id_text) {
    if (!std::isdigit(static_cast<unsigned char>(character))) {
      return false;
    }
  }

  out_id = std::stoi(id_text);
  const size_t rest_start = line.find_first_not_of(' ', close_bracket + 1);
  out_rest = (rest_start == std::string::npos) ?
      std::string() :
      line.substr(rest_start);
  return true;
}

bool tryParseStepperMotorLine(
    const std::string &line,
    int id,
    std::unique_ptr<Element> &out_element) {
  static const char *MOTOR_MARKER = "stepper motor ";
  static const char *REFERENCE_MARKER = "), ";
  static const char *DRIVEN_BY_MARKER = " driven by ";
  static const char *UART_MARKER = " (UART:";

  const size_t motor_pos = line.find(MOTOR_MARKER);
  if (motor_pos == std::string::npos) {
    return false;
  }

  const size_t index_start = motor_pos + strlen(MOTOR_MARKER);
  size_t index_end = index_start;
  while (index_end < line.size() &&
         std::isdigit(static_cast<unsigned char>(line[index_end]))) {
    ++index_end;
  }
  if (index_end == index_start) {
    return false;
  }
  const int index = std::stoi(line.substr(index_start, index_end - index_start));

  const size_t reference_marker_pos = line.find(REFERENCE_MARKER, index_end);
  if (reference_marker_pos == std::string::npos) {
    return false;
  }
  const size_t reference_start = reference_marker_pos + strlen(REFERENCE_MARKER);

  const size_t driven_by_pos = line.find(DRIVEN_BY_MARKER, reference_start);
  if (driven_by_pos == std::string::npos) {
    return false;
  }
  const std::string reference_name =
      line.substr(reference_start, driven_by_pos - reference_start);

  const size_t driver_start = driven_by_pos + strlen(DRIVEN_BY_MARKER);
  const size_t uart_pos = line.find(UART_MARKER, driver_start);
  const std::string driver_description = (uart_pos == std::string::npos) ?
      line.substr(driver_start) :
      line.substr(driver_start, uart_pos - driver_start);

  out_element = std::make_unique<SteperMotorElement>(
      id, index, reference_name, driver_description);
  return true;
}

bool tryParsePowerSupplyLine(
    const std::string &line,
    int id,
    std::unique_ptr<Element> &out_element) {
  if (line.find("power supply") == std::string::npos) {
    return false;
  }
  out_element = std::make_unique<PowerSupplyUnitElement>(id, true);
  return true;
}

bool tryParseServoLine(
    const std::string &line,
    int id,
    std::unique_ptr<Element> &out_element) {
  if (line.find("servo") == std::string::npos &&
      line.find("Servo") == std::string::npos) {
    return false;
  }
  out_element = std::make_unique<ServoElement>(id);
  return true;
}

}  // namespace

std::vector<std::unique_ptr<Element>> HardwareReportParser::parse(
    const std::vector<std::string> &lines,
    const std::string &title_prefix,
    int connection_id) {
  std::vector<std::unique_ptr<Element>> elements;

  for (const std::string &line : lines) {
    int id = -1;
    std::string rest;
    if (!tryParseElementId(line, id, rest)) {
      continue;
    }

    std::unique_ptr<Element> element;
    if (tryParseStepperMotorLine(rest, id, element) ||
        tryParseServoLine(rest, id, element) ||
        tryParsePowerSupplyLine(rest, id, element)) {
      element->setTitlePrefix(title_prefix);
      element->setConnectionId(connection_id);
      elements.push_back(std::move(element));
    }
  }

  return elements;
}
