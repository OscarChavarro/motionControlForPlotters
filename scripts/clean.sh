#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

rm -rf \
  "${repo_root}/build" \
  "${repo_root}"/build-* \
  "${repo_root}/platform/interactiveConsole/build" \
  "${repo_root}"/cmake-build-*

find "${repo_root}" -type f -name '*~' -delete

echo "Cleaned CMake build directories, interactive console build directory, CLion build directories, and editor backup files."
