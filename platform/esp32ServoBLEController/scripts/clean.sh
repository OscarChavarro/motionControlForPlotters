#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
build_dir="${ESP32_BUILD_DIR:-${project_dir}/build}"

case "${build_dir}" in
  ""|/|"${HOME}"|"${project_dir}")
    echo "Refusing to clean unsafe ESP32_BUILD_DIR '${build_dir}'." >&2
    exit 1
    ;;
esac

rm -rf "${build_dir}"
rm -f "${project_dir}/sdkconfig" "${project_dir}/sdkconfig.old"
find "${project_dir}" -type f -name '*~' -delete

echo "Cleaned ESP32 build directory, generated sdkconfig files, and editor backups."
