#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
idf_target="${IDF_TARGET:-esp32}"
build_dir="${ESP32_BUILD_DIR:-${project_dir}/build/${idf_target}}"
baud="${ESP32_BAUD:-115200}"
connect_attempts="${ESP32_CONNECT_ATTEMPTS:-0}"

if [ "${idf_target}" = "esp32" ]; then
  before_reset="no_reset"
  after_reset="no_reset"
else
  before_reset="default_reset"
  after_reset="hard_reset"
fi

# shellcheck source=esp-idf.sh
source "${script_dir}/esp-idf.sh"
find_idf

select_serial_port() {
  local ports=()
  local port

  if command -v python3 >/dev/null 2>&1 &&
      python3 -m serial.tools.list_ports >/dev/null 2>&1; then
    while IFS= read -r port; do
      ports+=("${port}")
    done < <(python3 -m serial.tools.list_ports | sed '/^[[:space:]]*$/d')
  else
    for port in \
      /dev/cu.usbmodem* \
      /dev/cu.usbserial* \
      /dev/cu.wchusbserial* \
      /dev/cu.SLAB_USBtoUART* \
      /dev/ttyUSB* \
      /dev/ttyACM*; do
      if [ -e "${port}" ]; then
        ports+=("${port}")
      fi
    done
  fi

  if [ "${#ports[@]}" -eq 0 ]; then
    echo "No USB serial port found. Connect the ESP32 and try again." >&2
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
    read -r -p "Select the ESP32 USB port number: " selection
    if [[ "${selection}" =~ ^[0-9]+$ ]] &&
        [ "${selection}" -ge 1 ] &&
        [ "${selection}" -le "${#ports[@]}" ]; then
      echo "${ports[$((selection - 1))]}"
      return 0
    fi
    echo "Invalid selection." >&2
  done
}

port="${1:-${ESP32_PORT:-}}"
if [ -z "${port}" ]; then
  port="$(select_serial_port)"
fi

python_executable="${IDF_PYTHON_ENV_PATH:-}/bin/python"
esptool_script="${IDF_PATH:-}/components/esptool_py/esptool/esptool.py"
if [ ! -x "${python_executable}" ] || [ ! -f "${esptool_script}" ]; then
  echo "ESP-IDF Python environment or esptool.py was not found." >&2
  return 1 2>/dev/null || exit 1
fi

idf.py -C "${project_dir}" -B "${build_dir}" \
  -DIDF_TARGET="${idf_target}" \
  -DSDKCONFIG="${build_dir}/sdkconfig" \
  build

echo "Flashing firmware through '${port}'..."
(
  cd "${build_dir}"
  "${python_executable}" "${esptool_script}" \
    --chip "${idf_target}" \
    --port "${port}" \
    --baud "${baud}" \
    --connect-attempts "${connect_attempts}" \
    --before "${before_reset}" \
    --after "${after_reset}" \
    write_flash \
    "@flash_args"
)

echo "Verifying flash contents against the generated binaries..."
(
  cd "${build_dir}"
  "${python_executable}" "${esptool_script}" \
    --chip "${idf_target}" \
    --port "${port}" \
    --baud "${baud}" \
    --connect-attempts "${connect_attempts}" \
    --before "${before_reset}" \
    --after "${after_reset}" \
    verify_flash \
    --diff yes \
    "@flash_args"
)

echo "ESP32 firmware for '${idf_target}' flashed and verified through '${port}'."
if [ "${idf_target}" = "esp32" ]; then
  echo "The board remains in the bootloader: release GPIO0 and perform a cold power cycle."
else
  echo "The board was reset after flashing."
fi
