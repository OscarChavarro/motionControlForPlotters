# AGENTS.md

Agent rules for this repository:

1. Do not use C++ namespaces in new or modified code.
2. Do not create commits with `git commit`.
3. The ESP32 board's onboard CP210x is damaged. Access UART0 through the FTDI
   adapter at `/dev/cu.usbserial-1120`. To identify the chip, first hold GPIO0
   to GND while resetting the board, then run:

   ```bash
   /Users/jedilink/.espressif/tools/python/v5.5.5/venv/bin/python /Users/jedilink/.espressif/v5.5.5/esp-idf/components/esptool_py/esptool/esptool.py --port /dev/cu.usbserial-1120 chip_id
   ```

   Do not flash the ESP32 without notifying the user first.

These rules apply to all agent-driven changes in this repository unless the user explicitly overrides them.
