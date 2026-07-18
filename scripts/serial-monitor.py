#!/usr/bin/env python3
import argparse
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
                data = connection.read(1024)
                if data:
                    sys.stdout.buffer.write(data)
                    sys.stdout.buffer.flush()
        except KeyboardInterrupt:
            return 0


if __name__ == "__main__":
    raise SystemExit(main())
