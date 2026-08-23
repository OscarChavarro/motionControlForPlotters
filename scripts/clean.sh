#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

rm -rf \
  "${repo_root}/build" \
  "${repo_root}"/build-* \
  "${repo_root}/platform/interactiveConsole/build" \
  "${repo_root}/platform/esp32ServoBLEController/build" \
  "${repo_root}"/cmake-build-*

rm -f \
  "${repo_root}/platform/esp32ServoBLEController/sdkconfig" \
  "${repo_root}/platform/esp32ServoBLEController/sdkconfig.old"

find "${repo_root}" -type f -name '*~' -delete

echo "Cleaned CMake, platform, ESP-IDF, CLion, and editor-generated files."
