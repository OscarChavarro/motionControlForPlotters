#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
preset="${MOTION_CONTROL_CMAKE_PRESET:-avr-uno-debug}"

cd "${repo_root}"

cmake --preset "${preset}"
cmake --build --preset "${preset}"

echo "Firmware HEX generated with preset '${preset}'."
