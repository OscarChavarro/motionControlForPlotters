#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
preset="${MOTION_CONTROL_CMAKE_PRESET:-avr-uno-debug}"
build_dir="${MOTION_CONTROL_BUILD_DIR:-${repo_root}/cmake-build-avr-uno-debug}"
baud="${ARDUINO_MONITOR_BAUD:-1000000}"

read_cache_value() {
  local key="$1"
  local cache_file="${build_dir}/CMakeCache.txt"

  if [ -f "${cache_file}" ]; then
    awk -F= -v key="${key}" '$1 ~ "^" key ":" {print $2; exit}' "${cache_file}"
  fi
}

select_serial_port() {
  local ports=()
  local port

  if python3 -m serial.tools.list_ports -v >/dev/null 2>&1; then
    while IFS= read -r port; do
      ports+=("${port}")
    done < <(python3 - <<'PY'
from serial.tools import list_ports

for port in list_ports.comports():
    device = port.device
    if (
        "usbmodem" in device
        or "usbserial" in device
        or device.startswith("/dev/ttyACM")
        or device.startswith("/dev/ttyUSB")
    ):
        print(device)
PY
)
  else
    for port in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/ttyACM* /dev/ttyUSB*; do
      if [ -e "${port}" ]; then
        ports+=("${port}")
      fi
    done
  fi

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

cmake --preset "${preset}" \
  -DARDUINO_PORT="${port}" \
  -DARDUINO_MONITOR_BAUD="${baud}" >/dev/null
cached_baud="$(read_cache_value "ARDUINO_MONITOR_BAUD")"
if [ -n "${cached_baud}" ]; then
  baud="${cached_baud}"
fi

if python3 -c "import serial" >/dev/null 2>&1; then
  python3 "${script_dir}/serial-monitor.py" "${port}" "${baud}"
else
  "${script_dir}/serial-monitor.sh" "${port}" "${baud}"
fi
