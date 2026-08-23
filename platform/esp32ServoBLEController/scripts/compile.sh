#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
idf_target="${IDF_TARGET:-esp32}"
build_dir="${ESP32_BUILD_DIR:-${project_dir}/build/${idf_target}}"

# shellcheck source=esp-idf.sh
source "${script_dir}/esp-idf.sh"
find_idf

idf.py -C "${project_dir}" -B "${build_dir}" \
  -DIDF_TARGET="${idf_target}" \
  -DSDKCONFIG="${build_dir}/sdkconfig" \
  build

echo "ESP32 firmware for '${idf_target}' built at '${build_dir}'."
