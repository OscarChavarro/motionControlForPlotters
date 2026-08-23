# ESP32 Servo BLE Controller

ESP-IDF firmware for ESP32-family devices with Bluetooth Low Energy support.
The default target is the classic dual-core ESP32 development board based on
the ESP-WROOM-32 module (including the AZDelivery clone). USB is only used to
flash the firmware. Runtime communication uses Bluetooth Low Energy.

The device name is `ESP-WROOM-32` and it implements the Nordic UART Service
(NUS). GPIO2 also drives a status LED, alternating between one second off and
one second on while the firmware is running. GPIO22 is configured as a 50 Hz
LEDC PWM output for the connected servo; `motorPosition` values from 0 to 255
are mapped to 1000 us to 2000 us servo pulses.

| Purpose | UUID | Properties |
|---|---|---|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | Primary service |
| Client to ESP32 (RX) | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` | Write, write without response |
| ESP32 to client (TX) | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` | Notify |

After connecting, the client must subscribe to TX notifications. The firmware
then sends `firmware boot`. Bytes written to RX are parsed as ASCII commands;
clients can send one complete command per BLE write or terminate streamed
commands with `\r` or `\n`. Long output is split into chunks that fit the
negotiated BLE ATT MTU.

Initial BLE commands:

```text
hardware
. [<id>]
servo <id> <position>
test off|on
```

`hardware` lists the GPIO22 servo PWM output and the GPIO2 heartbeat LED.
`.` prints the current servo telemetry summary. `. 0` prints the CSV status
for the servo PWM element and `. 1` prints the CSV status for the heartbeat
LED element. `servo 0 <position>` sets the servo resource to a value from 0 to
255 and disables the demo movement. `test off` stops the demo movement, while
`test on` enables it again.

## Requirements

- ESP-IDF 5.x with the toolchain for the selected `IDF_TARGET` installed
- CMake 3.22 or newer
- Ninja
- An FTDI UART adapter and a stable board power source for the current hardware

The scripts automatically discover installations made by EIM under
`~/.espressif`, common legacy installations under `~/esp/esp-idf` and
`~/esp-idf`, or an SDK selected explicitly through `IDF_PATH`.

## Build

From this directory:

```bash
./scripts/compile.sh
```

`IDF_TARGET` defaults to `esp32`. Set it to build for another supported chip;
each target uses a separate build directory and SDK configuration:

```bash
IDF_TARGET=esp32h2 ./scripts/compile.sh
```

From a configured repository root build, the equivalent target is:

```bash
cmake --build cmake-build-host-debug --target esp32_build
```

For a repository-level build, select the target while configuring CMake:

```bash
cmake -S . -B cmake-build-h2 -DIDF_TARGET=esp32h2
cmake --build cmake-build-h2 --target esp32_build
```

The BLE task uses core 1 on multicore configurations and no explicit affinity
on single-core configurations. GPIO2 remains the default status LED pin; board
variants whose onboard LED uses another GPIO still require a pin adjustment.

## Flash through USB

The current board's onboard CP210x bridge is damaged. Connect an FTDI adapter
to UART0 RX/TX and use `/dev/cu.usbserial-1120`. Cross the data lines (ESP32 TX0
to FTDI RX and ESP32 RX0 to FTDI TX), connect their grounds, and power the ESP32
from a stable source. Do not rely on a small FTDI adapter's 3.3 V output to
power the board.

The damaged USB/reset circuitry on this particular board also makes its RST
button unreliable: EN measures about 3.28 V at rest but only falls to about
1.3 V while RST is held. A valid manual reset is obtained by briefly connecting
EN directly to GND and then releasing it.

An external normally-open reset button can be installed as follows. The button
must connect EN to GND, never directly to 3.3 V. The capacitor is optional; if
the damaged automatic-reset branch continues driving EN, isolate that branch
before adding a stronger pull-up.

```text
                         ESP32
  +3.3 V ----[ 4.7 kΩ ]----+---- EN / CHIP_PU
                            |
                            +----||---- GND
                            |   100 nF
                            |
                         [ RESET ]
                      normally open
                            |
                           GND
```

To enter the immutable ROM bootloader:

1. Connect GPIO0 to GND.
2. Briefly connect EN to GND, then release EN.
3. Keep GPIO0 low while the flash tool connects and performs its operations.
4. Run the flash command below.

```bash
./scripts/send.sh /dev/cu.usbserial-1120
```

The script builds the project, writes the bootloader, partition table and
application, and then runs an independent `verify_flash` comparison against all
three generated binaries. A successful operation contains three `verify OK
(digest matched)` messages. It deliberately finishes with `--after no_reset`
so this board remains in the ROM bootloader instead of using its unreliable
reset circuitry. The default transfer rate is the conservative 115200 baud
because this board stopped responding during a 460800-baud transfer. Set
`ESP32_BAUD` explicitly only when using a known-stable UART connection. The
script also waits indefinitely for the manual ROM synchronization by default;
set `ESP32_CONNECT_ATTEMPTS` to a positive value to impose a retry limit.
Both write and verification use `--before no_reset`; EN and GPIO0 must be
operated manually because DTR/RTS reset sequences are unreliable on this board.

After verification, disconnect GPIO0 from GND, remove power for at least two
seconds, and restore power. A cold power cycle has been more reliable than the
board's RST button. A normal UART boot starts with:

```text
boot:0x13 (SPI_FAST_FLASH_BOOT)
```

The following output means GPIO0 was sampled low and the application will not
run:

```text
boot:0x3 (DOWNLOAD_BOOT...)
waiting for download
```

Repeated `rst:0x1 (POWERON_RESET)` messages indicate instability on EN or the
power/reset circuitry rather than a firmware crash.

Set `ESP32_PORT=/dev/...` or pass the port as the first argument to avoid
interactive port selection:

```bash
./scripts/send.sh /dev/cu.usbserial-0001
```

For non-`esp32` targets, the script selects the matching esptool chip, uses the
target-generated flash layout, and enables normal automatic reset handling:

```bash
IDF_TARGET=esp32h2 ./scripts/send.sh /dev/cu.usbmodem0001
```

The equivalent root target is `esp32_flash`. Disconnecting USB after flashing
does not affect BLE operation as long as the board has another power source.

## Hardware diagnosis from the MicroPython test

The following facts were verified on this specific board during bring-up:

- The chip is an ESP32-D0WDQ6 revision 1.0 with two cores and 4 MB of flash.
- The ROM bootloader, SPI flash and UART0 work. Written images passed an
  independent digest comparison against the source binaries.
- MicroPython 1.28.0 booted successfully and reached its UART REPL.
- A minimal `main.py` could drive GPIO2 high and blink an attached LED once it
  was explicitly started.
- GPIO2 read back as high and illuminated the LED, so the pin and LED wiring
  are operational.
- A cold power cycle with GPIO0 high produced `SPI_FAST_FLASH_BOOT` and allowed
  MicroPython to start `main.py` automatically.
- The unreliable EN/RST and GPIO0 boot-strapping behavior explains the earlier
  failures to observe either the GPIO2 heartbeat or BLE advertisements. Those
  failures do not by themselves implicate FreeRTOS, NimBLE or the C++ image.

At the end of this diagnostic test, MicroPython occupies the flash and the C++
firmware must be flashed again before testing BLE.

## Returning to the C++ firmware

Flashing the C++ image will overwrite MicroPython, including its filesystem and
`main.py`. This does not remove the ESP32 ROM bootloader: the ROM loader is part
of the chip and remains available regardless of what is stored in SPI flash.
Therefore a broken or non-booting C++ application can always be replaced by
entering download mode with GPIO0 and EN as described above.

Use this recovery and activation plan:

1. Build the C++ firmware with `./scripts/compile.sh`.
2. Enter the ROM bootloader using GPIO0 and a direct EN-to-GND reset.
3. Run `./scripts/send.sh /dev/cu.usbserial-1120` and require all three
   `verify OK` results before proceeding.
4. Disconnect GPIO0 from GND.
5. Remove power for at least two seconds and restore it; do not depend on the
   faulty RST button or on `esptool run` to launch the application.
6. Confirm the GPIO2 one-second heartbeat first, then scan for the BLE device
   name and Nordic UART Service UUID.
7. If the heartbeat is absent, capture UART0 during another cold power cycle.
   Diagnose the ROM boot mode before changing any C++ or FreeRTOS code.

## Clean

```bash
./scripts/clean.sh
```

This removes the ESP-IDF build tree plus generated `sdkconfig` files. The root
CMake target is `esp32_clean`.
