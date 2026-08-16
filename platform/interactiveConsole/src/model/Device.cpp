#include "Device.h"

#include <utility>

Device::Device(std::string hardware_description, std::string role)
    : hardware_description_(std::move(hardware_description)),
      role_(std::move(role)) {}

const std::string &Device::hardwareDescription() const {
  return hardware_description_;
}

const std::string &Device::role() const {
  return role_;
}

std::string Device::description() const {
  return "Device " + hardware_description_ + ", Role " + role_;
}

bool Device::parseDescription(const std::string &line, Device &device) {
  static const std::string DEVICE_PREFIX = "Device ";
  static const std::string ROLE_SEPARATOR = ", Role ";

  if (line.compare(0, DEVICE_PREFIX.size(), DEVICE_PREFIX) != 0) {
    return false;
  }

  const size_t role_position = line.find(ROLE_SEPARATOR, DEVICE_PREFIX.size());
  if (role_position == std::string::npos ||
      role_position == DEVICE_PREFIX.size() ||
      role_position + ROLE_SEPARATOR.size() == line.size()) {
    return false;
  }

  device = Device(
      line.substr(DEVICE_PREFIX.size(), role_position - DEVICE_PREFIX.size()),
      line.substr(role_position + ROLE_SEPARATOR.size()));
  return true;
}
