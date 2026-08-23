#include "BlePort.h"

std::unique_ptr<BlePort> BlePort::scanAndConnect(
    std::chrono::milliseconds timeout) {
  static_cast<void>(timeout);
  return nullptr;
}
