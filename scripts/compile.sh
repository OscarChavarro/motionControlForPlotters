#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
preset="${MOTION_CONTROL_CMAKE_PRESET:-avr-mega2560-debug}"
interactive_build_dir="${MOTION_CONTROL_INTERACTIVE_BUILD_DIR:-${repo_root}/platform/interactiveConsole/build}"

cd "${repo_root}"

cmake --preset "${preset}"
cmake --build --preset "${preset}"

cmake -S "${repo_root}/platform/interactiveConsole" \
  -B "${interactive_build_dir}" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build "${interactive_build_dir}"

echo "Firmware HEX generated with preset '${preset}'."
echo "Interactive console built at '${interactive_build_dir}'."
