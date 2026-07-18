#!/usr/bin/env python3
import argparse
import select
import sys

import serial


def main():
    parser = argparse.ArgumentParser(description="Simple pyserial monitor.")
    parser.add_argument("port")
    parser.add_argument("baud", type=int)
    args = parser.parse_args()

    with serial.Serial(
        port=args.port,
        baudrate=args.baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.2,
        write_timeout=0.2,
    ) as connection:
        try:
            connection.reset_input_buffer()
        except serial.SerialException:
            pass

        print(
            f"Serial monitor: {args.port} at {args.baud} baud, 8N1. "
            "Press Ctrl-C to exit.",
            file=sys.stderr,
        )

        try:
            while True:
                readable, _, _ = select.select([connection, sys.stdin], [], [])
                if connection in readable:
                    data = connection.read(1024)
                    if data:
                        sys.stdout.buffer.write(data)
                        sys.stdout.buffer.flush()
                if sys.stdin in readable:
                    line = sys.stdin.readline()
                    if line == "":
                        return 0
                    connection.write(line.encode("utf-8"))
                    connection.flush()
        except KeyboardInterrupt:
            return 0


if __name__ == "__main__":
    raise SystemExit(main())
