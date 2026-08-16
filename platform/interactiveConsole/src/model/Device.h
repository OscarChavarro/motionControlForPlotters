#pragma once

#include <string>

// A device announces the hardware on which its firmware runs and the role
// fulfilled by that firmware.  The serial protocol representation is kept in
// one place so every platform firmware can use the same contract.
class Device {
 public:
  Device(std::string hardware_description, std::string role);

  const std::string &hardwareDescription() const;
  const std::string &role() const;
  std::string description() const;

  static bool parseDescription(const std::string &line, Device &device);

 private:
  std::string hardware_description_;
  std::string role_;
};
