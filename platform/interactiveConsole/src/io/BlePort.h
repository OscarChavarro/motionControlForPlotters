#pragma once

#include <chrono>
#include <memory>
#include <string>

class BlePort {
 public:
  virtual ~BlePort() = default;

  BlePort(const BlePort &) = delete;
  BlePort &operator=(const BlePort &) = delete;

  static std::unique_ptr<BlePort> scanAndConnect(
      std::chrono::milliseconds timeout);

  virtual bool isOpen() const = 0;
  virtual std::string name() const = 0;
  virtual std::string readAvailable(bool &connection_lost) = 0;
  virtual bool writeText(const std::string &text) = 0;

 protected:
  BlePort() = default;
};
