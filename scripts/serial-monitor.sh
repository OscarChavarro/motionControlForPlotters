#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <serial-port> <baud-rate>" >&2
  exit 2
fi

port="$1"
baud="$2"
saved_tty_settings=""
platform="$(uname -s)"
reader_pid=""
cleaned_up=0
serial_fd_path=""

if [ ! -e "$port" ]; then
  echo "Serial port not found: $port" >&2
  exit 1
fi

restore_terminal() {
  if [ -n "${saved_tty_settings}" ]; then
    stty "${saved_tty_settings}" 2>/dev/null || true
  else
    stty sane 2>/dev/null || true
  fi
  printf '\033[0m\033[?25h\n' 2>/dev/null || true
}

stop_reader() {
  if [ -n "${reader_pid}" ] && kill -0 "${reader_pid}" 2>/dev/null; then
    kill "${reader_pid}" 2>/dev/null || true
    wait "${reader_pid}" 2>/dev/null || true
  fi
}

cleanup() {
  if [ "${cleaned_up}" -ne 0 ]; then
    return
  fi
  cleaned_up=1
  stop_reader
  exec 3<&- 2>/dev/null || true
  exec 3>&- 2>/dev/null || true
  restore_terminal
}

handle_interrupt() {
  cleanup
  exit 0
}

saved_tty_settings="$(stty -g 2>/dev/null || true)"
trap cleanup EXIT
trap handle_interrupt INT TERM

echo "Serial monitor: $port at $baud baud, 8N1. Press Ctrl-C to exit." >&2

exec 3<>"${port}"
if [ -e /dev/fd/3 ]; then
  serial_fd_path="/dev/fd/3"
else
  serial_fd_path="${port}"
fi

case "${platform}" in
  Darwin|*BSD)
    stty -f "${serial_fd_path}" "$baud" cs8 -cstopb -parenb raw -echo
    ;;
  Linux)
    stty -F "${serial_fd_path}" "$baud" cs8 -cstopb -parenb raw -echo
    ;;
  *)
    echo "Unsupported host platform for serial monitor: ${platform}" >&2
    exit 1
    ;;
esac

cat <&3 &
reader_pid="$!"

while kill -0 "${reader_pid}" 2>/dev/null; do
  if IFS= read -r line; then
    printf '%s\r\n' "${line}" >&3
  else
    break
  fi
done

exit 0
