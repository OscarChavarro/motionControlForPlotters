#include "SerialPort.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>
#endif

SerialPort::SerialPort(const char *path, int baud_rate)
    : file_descriptor_(-1), opened_(false) {
  open(path, baud_rate);
}

SerialPort::~SerialPort() {
  closePort();
}

bool SerialPort::open(const char *path, int baud_rate) {
  closePort();
  error_.clear();

  file_descriptor_ = ::open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (file_descriptor_ < 0) {
    error_ = std::string("Could not open ") + path + ": " + strerror(errno);
    return false;
  }

  termios options;
  if (tcgetattr(file_descriptor_, &options) != 0) {
    error_ = std::string("Could not read serial configuration: ") +
             strerror(errno);
    closePort();
    return false;
  }

  cfmakeraw(&options);
  speed_t speed;
  bool use_custom_baud_rate = false;
  if (!baudRateToSpeed(baud_rate, speed)) {
#ifdef __APPLE__
    speed = B9600;
    use_custom_baud_rate = true;
#else
    error_ = "Unsupported baud rate: " + std::to_string(baud_rate);
    closePort();
    return false;
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
    error_ = std::string("Could not configure serial port: ") +
             strerror(errno);
    closePort();
    return false;
  }

#ifdef __APPLE__
  if (use_custom_baud_rate) {
    speed_t custom_speed = static_cast<speed_t>(baud_rate);
    if (ioctl(file_descriptor_, IOSSIOSPEED, &custom_speed) != 0) {
      error_ = std::string("Could not configure serial speed: ") +
               strerror(errno);
      closePort();
      return false;
    }
  }
#else
  static_cast<void>(use_custom_baud_rate);
#endif

  tcflush(file_descriptor_, TCIFLUSH);
  opened_ = true;
  return true;
}

void SerialPort::closePort() {
  if (file_descriptor_ >= 0) {
    close(file_descriptor_);
    file_descriptor_ = -1;
  }
  opened_ = false;
}

bool SerialPort::isOpen() const {
  return opened_;
}

const std::string &SerialPort::error() const {
  return error_;
}

std::string SerialPort::readAvailable(bool &connection_lost) {
  std::string output;
  std::array<char, 1024> buffer;
  connection_lost = false;

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
    connection_lost = true;
    closePort();
    break;
  }

  return output;
}

bool SerialPort::writeText(const std::string &text) {
  if (file_descriptor_ < 0) {
    return false;
  }

  size_t offset = 0;
  while (offset < text.size()) {
    ssize_t bytes_written = write(
        file_descriptor_,
        text.data() + offset,
        text.size() - offset);
    if (bytes_written > 0) {
      offset += static_cast<size_t>(bytes_written);
      continue;
    }
    if (bytes_written < 0 && errno == EINTR) {
      continue;
    }
    return false;
  }
  return true;
}

bool SerialPort::baudRateToSpeed(int baud_rate, speed_t &speed) {
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
#ifdef B250000
    case 250000:
      speed = B250000;
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
