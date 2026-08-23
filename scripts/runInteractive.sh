#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
preset="${MOTION_CONTROL_CMAKE_PRESET:-avr-mega2560-debug}"
build_dir="${MOTION_CONTROL_INTERACTIVE_BUILD_DIR:-${repo_root}/platform/interactiveConsole/build}"
baud="${ARDUINO_MONITOR_BAUD:-1000000}"

read_cache_value() {
  local key="$1"
  local cache_file="${repo_root}/cmake-build-avr-mega2560-debug/CMakeCache.txt"

  if [ -f "${cache_file}" ]; then
    awk -F= -v key="${key}" '$1 ~ "^" key ":" {print $2; exit}' "${cache_file}"
  fi
}

find_serial_ports() {
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
    return 0
  fi

  printf '%s\n' "${ports[@]}"
}

if [ "$#" -gt 1 ]; then
  echo "Usage: $0 [baud-rate]" >&2
  exit 2
fi

if [ "$#" -eq 1 ]; then
  baud="$1"
else
  cached_baud="$(read_cache_value "ARDUINO_MONITOR_BAUD")"
  if [ -n "${cached_baud}" ]; then
    baud="${cached_baud}"
  fi
fi

ports=()
while IFS= read -r port; do
  ports+=("${port}")
done < <(find_serial_ports)

cd "${repo_root}"

cmake --preset "${preset}" >/dev/null
cmake -S "${repo_root}/platform/interactiveConsole" \
  -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "${build_dir}" >/dev/null

if [ "${#ports[@]}" -eq 0 ]; then
  exec "${build_dir}/interactiveConsole" "${baud}"
else
  exec "${build_dir}/interactiveConsole" "${baud}" "${ports[@]}"
fi
