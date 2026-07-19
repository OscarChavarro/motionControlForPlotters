# Motion Control Platform Architecture Notes

## Project Vision

This project is not intended to be an Arduino sketch or a GRBL clone. The objective is to build a portable, modular motion-control framework capable of running on multiple embedded platforms while being driven by a sophisticated CAD backend.

Primary goals:

- C++-based architecture.
- No Arduino IDE.
- Git repository with CLI workflow.
- Portable core.
- Hardware Abstraction Layer (HAL).
- Binary communication protocol.
- Interrupt- and timer-based execution (no `delay()`).
- Closed-loop validation using computer vision.

## Target Platforms

- Arduino AVR (Mega/Uno family)
- ESP32
- Linux-based dual-core RISC-V development board (64 MB RAM)

The same motion core should execute on every platform, with only the HAL changing.

## Current Implementation Phase

The repository is currently building the infrastructure incrementally. The
Arduino program is diagnostic firmware, not the final motion-control protocol
or planner. Its repeating forward/backward movement exists to exercise motor
modeling, physical pin assignment, motion profiles, power gating, and
telemetry while those pieces are separated into reusable layers.

The current boundary is:

```text
main.cpp motor registry and diagnostic program
    -> portable HAL interfaces
        -> SystemClock, UartSerial, GpioLed,
           ExternalPowerSupplyDetector, StepperMotorDriver
    -> portable motion logic
        -> StepperMotorController (per-motor state and motion profile)
    -> AVR implementations created during platform initialization
        -> AvrSystemClock, AvrUartSerial, AvrGpioLed,
           AvrExternalPowerSupplyDetector, AvrStepDirectionDriver
```

The diagnostic program constructs each concrete `Avr*` object once and then
uses references to the corresponding core interface. This keeps application
logic independent from AVR headers and allows another platform to compose the
same logic with its own implementations.

`StepperMotor` describes one physical motor installation: motor reference,
motor type, full steps per rotation, configured microstepping, and its `STEP`
and `DIR` pin assignment. Each registered motor owns an independent driver and
controller state, including position, direction, speed, profile clock, and
fractional-step accumulator.

On an Arduino Mega 2560, reserving D0 and D1 for UART leaves D2-D53 available
for up to 26 two-pin `STEP`/`DIR` motor drivers. The AVR platform driver maps
that complete digital-pin range. This is a pin-count limit only: the present
cooperative loop emits pulses with short blocking waits. A timer/interrupt
step scheduler is still required before simultaneous high-rate operation can
be guaranteed for many motors.

## Development Environment

- VS Code
- PlatformIO for embedded builds
- Git
- C++
- Optional CMake for the reusable core

Suggested repository layout:

```text
motion-core/
firmware-avr/
firmware-esp32/
firmware-riscv/
host/
docs/
tests/
```

The reusable motion core should remain independent from PlatformIO.

## Motion Architecture

```text
CAD Kernel
    ↓
Hidden Line Removal
    ↓
Motion Backend
    ↓
Binary Protocol
    ↓
Motion Controller
    ↓
Motion Planner
    ↓
Rasterization (Bresenham)
    ↓
Step Scheduler
    ↓
HAL
    ↓
Stepper Drivers
```

## PC Responsibilities

The PC already implements:

- Winged-edge B-Rep kernel
- Euler operators
- Splitting operations
- Boolean operations
- Camera projection
- Quantitative observability
- Appel hidden-line algorithm

The CAD kernel generates projected 2D primitives that will drive the plotter.

The firmware never manipulates CAD topology.

## Firmware Responsibilities

The firmware is responsible for:

- Parsing binary packets
- Queueing primitives
- Motion planning
- Rasterization
- Pulse generation
- Sensor handling
- Safety

## Primitive-Based Protocol

The protocol transports graphical primitives rather than low-level motor commands.

Examples:

- MOVE_TO
- LINE_TO
- ARC
- BEZIER
- PEN_UP
- PEN_DOWN
- HOME
- STOP
- PANIC

The protocol is binary, versioned, little-endian, and transport-independent (USB, TCP, BLE, Wi-Fi, UART).

Each command should include a transaction ID to correlate firmware execution with camera observations.

## Rasterization

The firmware should execute Bresenham internally.

Curves should be converted into line segments before Bresenham.

Examples:

Bezier
→ Adaptive subdivision
→ Line segments
→ Bresenham

Arc
→ Arc subdivision
→ Line segments
→ Bresenham

## Motion Generation

The firmware should use:

- Hardware timers
- Interrupts
- State machines
- Pulse scheduling

The final scheduler must not use blocking delays. The current diagnostic AVR
driver still uses short blocking waits while emitting each pulse; replacing it
with timer/interrupt scheduling is part of the planned platform work.

Future extensions:

- Command-driven trapezoidal profiles (a repeating diagnostic profile exists)
- S-curve acceleration
- Look-ahead planner

## Safety

Firmware states:

- BOOT
- INITIALIZING
- IDLE
- READY
- RUNNING
- PAUSED
- HOMING
- ERROR
- PANIC

PANIC immediately:

- Stops motion timers
- Clears queues
- Disables drivers
- De-energizes motors

Recovery requires RESET followed by HOMING.

## Limit Switches

Support:

- Home switches
- Safety switches
- Probe inputs
- Soft limits

Soft limits should exist both in the PC and firmware.

## Events

Firmware should report binary events instead of text:

- READY
- HOME_COMPLETE
- MOTION_COMPLETE
- LIMIT_REACHED
- EMERGENCY_STOP
- PANIC
- POSITION_LOST

## Closed-Loop Vision

Unlike traditional CNC controllers, this project includes vision-based validation.

The PC contains:

- OpenCV
- High-speed camera
- AprilTag fiducial tracking

Workflow:

1. PC sends a primitive.
2. Firmware executes it.
3. Camera measures the physical position.
4. PC validates the result.
5. PC either:
   - continues,
   - sends a correction,
   - retries,
   - or enters PANIC mode.

This creates a distributed control system where the firmware guarantees deterministic execution while the PC closes the feedback loop through computer vision.

## Long-Term Goal

Develop a modern, portable motion-control platform that cleanly separates:

- CAD
- Communication
- Motion planning
- Hardware abstraction
- Safety
- Vision feedback

The architecture should scale from AVR to ESP32 and Linux-based RISC-V systems without changing the core motion logic.
