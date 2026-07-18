# Motion Control for Plotters

Motion Control for Plotters is a C++17 firmware-oriented project for building a portable motion-control core and platform-specific embedded targets for plotter hardware.

The current codebase is split into two main areas:

- `motionControlCore`: reusable platform-independent motion logic.
- `platform`: hardware-specific firmware entry points and device wrappers.

At this stage, the core contains a 2D rasterization module and the platform layer contains an Arduino AVR firmware target. The Arduino firmware initializes a millisecond system clock, a UART serial interface, and the board LED, then runs a simple non-blocking blink loop that reports LED state over serial.

## Core

`motionControlCore` currently provides `Rasterizer2D`, a callback-based rasterization utility intended for plotter-oriented motion code.

It supports:

- Drawing integer 2D lines with a Bresenham-style algorithm.
- Drawing polygon outlines by rasterizing each polygon edge.
- Filling polygons by emitting horizontal spans through a callback.

The rasterizer does not draw into a framebuffer. Instead, callers provide callbacks that receive generated points or spans. This keeps the core independent from any display, image, or hardware implementation and makes it suitable for later connection to motion planners, step schedulers, or other plotter backends.

## Platform

The implemented platform target is `platform/arduinoStepMotorsXYController`, which builds AVR firmware without the Arduino IDE. Its platform-specific C++ code lives directly under `platform/arduinoStepMotorsXYController/src`.

It provides:

- `SystemClock`: configures AVR Timer0 compare interrupts to maintain a millisecond counter.
- `UartSerial`: configures UART0 and writes serial text output.
- `GpioLed`: controls the built-in board LED using AVR GPIO registers.
- `main.cpp`: initializes the platform services, prints boot messages, toggles the LED every second, and writes `LED=ON` / `LED=OFF` status messages over serial.

Supported board presets are:

- `uno`
- `nano`
- `mega2560`

## Install

Prerequisites:

- CMake 3.22 or newer.
- `avr-gcc`.
- AVR binutils and runtime libraries.
- `avrdude` for uploading firmware to an Arduino board.
- `make` or another CMake-supported build tool.

macOS:

```bash
brew tap osx-cross/avr;brew trust --formula osx-cross/avr/avr-gcc@9;brew trust osx-cross/avr;brew install avr-gcc avrdude
```

Ubuntu Linux:

```bash
sudo apt-get update
sudo apt-get install cmake gcc-avr avr-libc binutils-avr avrdude make
```

## Build

Configure the project from the repository root:

```bash
cmake -S . -B build
```

Build the reusable core and configure/build the Arduino firmware:

```bash
cmake --build build --target arduino_build
```

Generate the Arduino Intel HEX firmware:

```bash
cmake --build build --target arduino_hex
```

The generated firmware is written to:

```text
build/platform-arduinoStepMotorsXYController/arduinoStepMotorsXYController.hex
```

## Upload

Connect the Arduino board and configure the serial port if needed:

```bash
cmake -S . -B build -DARDUINO_BOARD=uno -DARDUINO_PORT=/dev/ttyACM0
cmake --build build --target arduino_upload
```

On macOS, the port will commonly look like `/dev/cu.usbmodem...` or `/dev/cu.usbserial...`.

## Serial Monitor

Open the serial monitor through the CMake target:

```bash
cmake --build build --target arduino_monitor
```

The monitor target uses `pyserial` miniterm when available, then falls back to `picocom` or the repository's shell serial monitor.

## Configuration

The top-level CMake project exposes these Arduino-related options:

- `ARDUINO_BOARD`: board preset, one of `uno`, `nano`, or `mega2560`.
- `ARDUINO_PORT`: serial/upload port.
- `ARDUINO_UPLOAD_BAUD`: optional upload baud override.
- `ARDUINO_MONITOR_BAUD`: optional serial monitor baud override.
- `ARDUINO_PROGRAMMER`: optional `avrdude` programmer override.

The Arduino subproject also accepts lower-level overrides such as `ARDUINO_MCU` and `ARDUINO_F_CPU` when a preset is not enough.
