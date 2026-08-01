#pragma once

#include <string>
#include <vector>

// Base interface for anything the console models in parallel to a piece of
// state tracked by the firmware (a motor, a power supply, etc). Every
// element mirrors a firmware-assigned numeric id, used by the "get <id>"
// command to fetch its live status.
class Element {
 public:
  explicit Element(int id) : id_(id) {
  }

  virtual ~Element() = default;

  int id() const {
    return id_;
  }

  // Short heading shown atop the element's box.
  virtual std::string title() const = 0;

  // Body lines shown inside the element's box.
  virtual std::vector<std::string> infoLines() const = 0;

  // Applies one CSV status line received from "get <id>". Read-only
  // elements can ignore this (default no-op).
  virtual void applyStatusLine(const std::string &line) {
    static_cast<void>(line);
  }

  // Consumes a keypress meant to control this element while selected
  // (LEFT/RIGHT move focus between the element's own widgets, space
  // activates the focused widget). Returns true if the key was consumed.
  // When activating a widget produces a firmware command, it is written
  // to out_command so the caller can send it over the serial link.
  // Read-only elements never consume anything (default no-op).
  virtual bool handleControlKey(int key, std::string &out_command) {
    static_cast<void>(key);
    static_cast<void>(out_command);
    return false;
  }

 private:
  int id_;
};
