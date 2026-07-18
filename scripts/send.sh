#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
preset="${MOTION_CONTROL_CMAKE_PRESET:-avr-mega2560-debug}"
build_dir="${MOTION_CONTROL_BUILD_DIR:-${repo_root}/cmake-build-avr-mega2560-debug}"

select_serial_port() {
  local ports=()
  local port

  for port in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/ttyACM* /dev/ttyUSB*; do
    if [ -e "${port}" ]; then
      ports+=("${port}")
    fi
  done

  if [ "${#ports[@]}" -eq 0 ]; then
    echo "No serial ports found. Connect the board and try again." >&2
    return 1
  fi

  if [ "${#ports[@]}" -eq 1 ]; then
    echo "${ports[0]}"
    return 0
  fi

  echo "Available serial ports:" >&2
  local index=1
  for port in "${ports[@]}"; do
    echo "  ${index}) ${port}" >&2
    index=$((index + 1))
  done

  local selection
  while true; do
    read -r -p "Select serial port number: " selection
    if [[ "${selection}" =~ ^[0-9]+$ ]] &&
      [ "${selection}" -ge 1 ] &&
      [ "${selection}" -le "${#ports[@]}" ]; then
      echo "${ports[$((selection - 1))]}"
      return 0
    fi
    echo "Invalid selection." >&2
  done
}

port="$(select_serial_port)"

cd "${repo_root}"

cmake --preset "${preset}" -DARDUINO_PORT="${port}"
cmake --build --preset "${preset}"
cmake --build "${build_dir}" --target arduino_upload

echo "Firmware sent to ${port}."
