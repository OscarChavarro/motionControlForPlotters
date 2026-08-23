#!/usr/bin/env bash

find_idf() {
  if command -v idf.py >/dev/null 2>&1; then
    return 0
  fi

  local activation_script=""
  local candidate

  # ESP-IDF Installation Manager (EIM) creates versioned activation scripts.
  # Prefer the lexically greatest installed version when none is active.
  for candidate in "${HOME}/.espressif/tools"/activate_idf_*.sh; do
    if [ -f "${candidate}" ]; then
      activation_script="${candidate}"
    fi
  done

  if [ -n "${activation_script}" ]; then
    local eim_tool_path=""
    local environment_entry
    while IFS= read -r environment_entry; do
      case "${environment_entry}" in
        PATH=*)
          eim_tool_path="${environment_entry#PATH=}"
          ;;
        SYSTEM_PATH=*)
          ;;
        *=*)
          export "${environment_entry}"
          ;;
      esac
    done < <("${activation_script}" -e)

    export PATH="${eim_tool_path}:${PATH}"
    idf.py() {
      "${IDF_PYTHON_ENV_PATH}/bin/python" "${IDF_PATH}/tools/idf.py" "$@"
    }
    return 0
  fi

  for candidate in \
    "${IDF_PATH:-}/export.sh" \
    "${HOME}/esp/esp-idf/export.sh" \
    "${HOME}/esp-idf/export.sh" \
    "/opt/esp-idf/export.sh" \
    "/opt/homebrew/share/esp-idf/export.sh"; do
    if [ -n "${candidate}" ] && [ -f "${candidate}" ]; then
      activation_script="${candidate}"
      break
    fi
  done

  if [ -z "${activation_script}" ]; then
    echo "ESP-IDF was not found." >&2
    echo "Install ESP-IDF, source its export.sh, or set IDF_PATH." >&2
    return 1
  fi

  # ESP-IDF's export script configures PATH and its Python environment.
  # shellcheck disable=SC1090
  set +u
  source "${activation_script}" >/dev/null
  set -u
}
