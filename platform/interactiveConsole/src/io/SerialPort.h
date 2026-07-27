#pragma once

#include <string>
#include <termios.h>

class SerialPort {
 public:
  SerialPort(const char *path, int baud_rate);
  ~SerialPort();

  SerialPort(const SerialPort &) = delete;
  SerialPort &operator=(const SerialPort &) = delete;

  bool open(const char *path, int baud_rate);
  void closePort();
  bool isOpen() const;
  const std::string &error() const;

  std::string readAvailable(bool &connection_lost);
  bool writeText(const std::string &text);

 private:
  static bool baudRateToSpeed(int baud_rate, speed_t &speed);

  int file_descriptor_;
  bool opened_;
  std::string error_;
};
