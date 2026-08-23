#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cache_dir="${TMPDIR:-/tmp}/motion-control-swift-module-cache"
mkdir -p "${cache_dir}"

export CLANG_MODULE_CACHE_PATH="${cache_dir}"
exec /usr/bin/swift "${script_dir}/scan.swift"
