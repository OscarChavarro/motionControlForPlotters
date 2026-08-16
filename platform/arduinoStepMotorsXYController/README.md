# Arduino Step Motors XY Controller

Firmware target for an Arduino UNO or Mega2560-based XY step motor controller.

# Elements used

## Artillery D42HSA5401-23B stepper motor

NEMA17 "pancake" bipolar stepper motor, used for the Artillery Sidewinder
X1/X2 and Genius 3D printer extruders.

| Spec | Value |
|---|---|
| Model | D42HSA5401-23B |
| Motor type | NEMA17 bipolar hybrid stepping motor, "pancake" (short body) |
| Step angle | 1.8 degrees (200 steps/revolution) |
| Rated current | 0.8 A |
| Holding torque | 0.12 N.m |
| Shaft diameter | 5.0 mm |
| Body length | 22 mm |

Source: [Artillery Sidewinder X1/X2 Genius Extruder Nema17 Stepper Motor (D42HSA5401-23B)](https://www.amazon.es/Artillery-Sidewinder-Extrusor-Stepper-D42HSA5401-23B/dp/B0B26XPB9P)

## TMC2209 V2.0 stepper driver (generic clone)

Generic "TMC2209 V2.0" driver module, pin-compatible with the A4988/DRV8825
socket footprint, used to replace the A4988 driver.

| Spec | Value |
|---|---|
| Model | TMC2209 V2.0 (generic clone, sold as a 2-piece pack) |
| Driver chip | TRINAMIC/Analog Devices TMC2209 |
| Sense resistor | 0.11 ohm (R110) |
| Max current | 1.77 A RMS at VREF=2.5V (practical modules typically limited to ~1.2 A RMS) |
| Microstepping | Up to 1/256 via MicroPlyer interpolation, MS1/MS2 pins select 1/8, 1/16, 1/32, 1/64 in standalone mode |
| Chopper modes | StealthChop2 (silent) and SpreadCycle, selectable via SPREAD pin |
| Control interface | STEP/DIR, single-wire UART, analog VREF current scaling |

Source: [2 piezas de controladores de motor paso a paso TMC2209 V2.0](https://www.amazon.es/dp/B0D6R7YCRT)

## AZDelivery AZ-MEGA2560 microcontroller board (Arduino Mega 2560 clone)

Arduino Mega 2560-compatible board used as the firmware target.

| Spec | Value |
|---|---|
| Model | AZDelivery AZ-MEGA2560-Board with USB-C |
| Microcontroller | ATmega2560 |
| Flash memory | 256 KB (8 KB used by bootloader) |
| SRAM | 8 KB |
| EEPROM | 4 KB |
| Digital I/O pins | 54 (14 usable as PWM) |
| Analog inputs | 16 |
| UARTs | 4 |
| Clock speed | 16 MHz |
| USB interface | USB-C, CH340 converter |
| External power input | 5.5 x 2.1 mm jack, 6-15V (7-12V recommended) |

Source: [AZDelivery AZ-MEGA2560 Board Mikrokontroller mit USB-C Anschluss](https://www.amazon.es/dp/B0DHY38ZB1)

## LOMVUM T28B digital multimeter

Used for VREF calibration and for driver/motor temperature checks, via its
K-type thermocouple probe.

| Spec | Value |
|---|---|
| Model | LOMVUM T28B |
| Type | Auto-ranging digital multimeter, True RMS, 6000 counts |
| DC voltage | Up to 1000 V |
| AC voltage | Up to 750 V |
| AC/DC current | Up to 10 A |
| Capacitance | Up to 100 mF |
| Temperature probe | K-type thermocouple |
| Safety rating | CAT III 1000V / CAT IV 600V |
| Display | 3.0" backlit LCD, data hold |

Source: [LOMVUM T28B Automatic Digital Multimeter](https://www.amazon.es/dp/B07FXH2R5M)

## RUZIZAO regulated bench power supply

Adjustable bench power supply used to provide the external `VMOT` motor
supply voltage, with digital display, rotary encoder setpoint controls, and a
built-in USB fast-charging output.

| Spec | Value |
|---|---|
| Model | RUZIZAO Regulated Power Supply |
| Output voltage | 0-30 V, adjustable |
| Output current | 0-10 A, adjustable |
| USB output | 18 W fast charging |
| Protection | OCP (over-current), OVP (over-voltage) |
| Display | Digital, with rotary encoder setpoint controls |

Source: [RUZIZAO Regulated Power Supply 30V 10A](https://www.amazon.es/dp/B0DMW7M7PS)

Fritzing part: no exact match found; the closest available part is the
community-contributed [Hanmatek HM305 bench power supply](https://forum.fritzing.org/t/dc-power-supply/20680)
(`hanmatek-HM305.fzpz`, direct download at
[forum.fritzing.org/uploads/short-url/165qJwjxYqDpQ5ndsKmUOZ4f1SG.fzpz](https://forum.fritzing.org/uploads/short-url/165qJwjxYqDpQ5ndsKmUOZ4f1SG.fzpz)),
which shares the same boxed form factor: digital display, voltage/current
adjustment knobs, and DC output terminals. Import it via **Part > Import...**
in Fritzing.

# Arduino PINOUT

| Pin | UNO | MEGA2560 | Direction | Use |
|---|---|---|---|---|
| 2 | D2 / PD2 | D2 / PE4 | Output | A4988/TMC2209 STEP input |
| 3 | D3 / PD3 | D3 / PE5 | Output | Y-axis A4988 STEP input |
| 5 | D5 / PD5 | D5 / PE3 | Output | A4988/TMC2209 DIR input |
| 6 | D6 / PD6 | D6 / PH3 | Output | Y-axis A4988 DIR input |
| 8 | D8 / PB0 | D8 / PH5 | Output | Shared motor driver EN input (active low; CNC Shield X/Y/Z EN) |
| 13 | D13 / PB5 | D13 / PB7 | Output | Testing LED |
| A5 | ADC5 / PC5 | ADC5 / PF5 | Analog input | External power supply unit detection via voltage divider from VMOT |

This pinout matches the X-axis socket of a Protoneer-compatible Arduino CNC
Shield V3 (`doc/references/electronicsElements/arduino-cnc-shield-protoneer.pdf`):
STEP=D2, DIR=D5, and the shared EN=D8. The driver receives one motor step on
each rising edge of `STEP`. The `DIR` signal selects the direction used by the
next `STEP` rising edge. `EN` is active low and shared by every axis on the
shield: driving it LOW enables all motor drivers at once, HIGH disables them
all, regardless of each motor's own STEP/DIR state. `D0` and `D1` stay free
for the USB serial console.

The X registry entry uses a `D42HSC4409B-23B` motor driven by an Artillery
`FS31W01 191202` module in STEP/DIR mode. The second entry is wired to the
CNC Shield Y socket: STEP=D3 and DIR=D6; it uses the same motor and driver
identification. Both drivers are configured at 1/16 microstepping, so each
200-step motor reports 3200 microsteps per rotation. Their test segments are
two rotations. The shared D8 enable line controls both X and Y drivers.

The PSU voltage divider uses `A5` rather than `A0` on purpose: on the CNC
Shield, `A0`-`A3` are committed to Abort/Feed Hold/Cycle Start/Coolant, and
`A0` specifically already has the shield's own 10k pull-up populated, which
would bias this divider's reading if reused. `A4`/`A5` are documented as
unused/reserved on the shield, so `A5` is free of both the semantic and the
electrical conflict.

`AvrStepDirectionDriver` supports D2-D13 on UNO/Nano and D2-D53 on Mega
2560. The program rejects D0/D1 because this firmware reserves them for UART,
and it rejects repeated pins across registered motors. Consequently, the Mega
has a theoretical maximum of 26 two-pin motor drivers. The current
cooperative, blocking pulse implementation does not yet guarantee that 26
motors can be driven simultaneously at useful rates.

The first stepper motor and its controller are configured with:

```bash
-DSTEPPER_MOTOR_STEP_PIN=2
-DSTEPPER_MOTOR_DIRECTION_PIN=5
-DSTEPPER_MOTOR_Y_STEP_PIN=3
-DSTEPPER_MOTOR_Y_DIRECTION_PIN=6
-DSTEPPER_MOTOR_Y_DIRECTION_INVERTED=0
-DSTEPPER_MOTOR_Y_FULL_STEPS_PER_ROTATION=200
-DSTEPPER_MOTOR_Y_TRAVEL_ROTATIONS=2
-DMOTOR_DRIVER_ENABLE_PIN=8
-DSTEPPER_MOTOR_STEP_PULSE_MICROSECONDS=5
-DSTEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS=5
-DSTEPPER_MOTOR_TRAVEL_ROTATIONS=2
-DSTEPPER_MOTOR_ACCELERATION_MILLISECONDS=1000
-DSTEPPER_MOTOR_CRUISE_MILLISECONDS=8000
-DSTEPPER_MOTOR_DECELERATION_MILLISECONDS=1000
```

The example motion profile is a repeating trapezoid. In each direction the
motor moves 2 rotations in 10 seconds: 1 second accelerating, 8 seconds at
constant speed, and 1 second decelerating. The next 10-second segment repeats
the same trapezoid backward for 2 rotations. With the default 200-step
`Artillery D42HSA5401-23B` motor and the A4988 configured for 1/16
microstepping, 2 rotations map to 6400 microsteps and require a calculated
maximum profile speed of about 0.222 rotations per second.
The firmware stores motor definitions in the `STEPPER_MOTOR_PROGRAMS` table in
`src/main.cpp`. Adding one table entry automatically adds its platform driver,
portable controller, initialization, validation, update, and telemetry state.
Each table entry may have independent model, pins, pulse timings, travel, and
trapezoid timings.

The reusable `StepperMotorController` and the `SystemClock`, `UartSerial`,
`GpioLed`, `ExternalPowerSupplyDetector`, `MotorDriverEnable`, and
`StepperMotorDriver` interfaces live in `motionControlCore`. Their concrete
implementations in this target are named `AvrSystemClock`, `AvrUartSerial`,
`AvrGpioLed`, `AvrExternalPowerSupplyDetector`, `AvrMotorDriverEnable`, and
`AvrStepDirectionDriver`. `main.cpp` creates these concrete objects during
initialization and uses the core interface types afterward.

The external power supply detector no longer measures a voltage: it reads a
`PC817` opto-isolator wired to `A5`, which reports only presence/absence of
the external `VMOT` supply.

```text
VMOT --/\/\/-- (1)PC817(2) -- GND      LED side: lit whenever VMOT is present
  2.2 kOhm

 5V (internal pull-up) --- A5 ---(3)PC817(4)--- GND   Phototransistor side
```

Wiring:

| PC817 pin | Connects to | Role |
|---|---|---|
| 1 (LED anode) | `VMOT` through a `2.2 kOhm` series resistor | Lights the opto LED whenever the external supply is present |
| 2 (LED cathode) | `GND` | LED return path |
| 3 (phototransistor) | `A5` | Pulled toward `GND` while the LED is lit |
| 4 (phototransistor) | `GND` | Phototransistor return path |

`A5` is configured as a digital input with the ATmega's internal pull-up
enabled, so it idles HIGH (~5V) with no external pull-up resistor needed.
Reading is **inverted logic**: `LOW` (~0V) means the opto LED is lit and the
external supply is present; `HIGH` (~5V, pulled up) means it is absent. This
also sidesteps the analog-divider conflict with the CNC Shield's `A0`-`A3`
pins discussed above: `A5` here just needs a clean digital HIGH/LOW, not an
analog measurement.

Since there is no measured voltage anymore, the millivolt-returning methods
on `AvrExternalPowerSupplyDetector` (`readAnalogInputMilliVolts`,
`readExternalSupplyMilliVolts`, `filteredExternalSupplyMilliVolts`) report a
nominal value instead of a measurement: `EXTERNAL_VOLTAGE_PSU * 1000` when
present, `0` when absent. This keeps the existing telemetry, `EVENT
PSU=READY|LOST VMOTOR=<voltage>V` line, and `PSU,<ON|OFF>,<voltage>,...` CSV
format unchanged, even though `<voltage>` is now a configured nominal value
rather than something actually measured.

| Variable | Default | Used for |
|---|---:|---|
| `EXTERNAL_VOLTAGE_PSU` | `24` | Nominal external power supply voltage, in volts, reported when the opto-isolator detects `VMOT` present |
| `PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL` | `5000` | Minimum interval between missing-PSU error messages, in milliseconds |

The nominal PSU voltage is configured at CMake level with:

```bash
-DEXTERNAL_VOLTAGE_PSU=24
```

The missing-PSU error print interval is configured in milliseconds with:

```bash
-DPSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL=5000
```

At runtime, `AvrExternalPowerSupplyDetector` samples the digital `A5` reading
every 10 ms and requires 10 consecutive consistent samples (100 ms) before
flipping `isExternalPowerSupplyAvailable()`, debouncing the opto-isolator's
transition instead of filtering a measured voltage against a threshold.

Confirmed supply transitions produce a concise event containing the nominal
voltage value:

```text
EVENT PSU=READY VMOTOR=<voltage>V
EVENT PSU=LOST VMOTOR=<voltage>V
```

## Motor driver enable (`motorDriverEnabled`)

The PSU element also models a `motorDriverEnabled` flag, backed by
`MOTOR_DRIVER_ENABLE_PIN` (`D8` by default): the shared, active-low `EN` line
that every motor driver on the CNC Shield listens to at once.

- The flag defaults to `true`.
- The pin is only actually driven active (LOW, drivers powered) when **both**
  the flag is `true` **and** the external power supply is present. With no
  PSU, the drivers are always disabled regardless of the flag: there is
  nothing safe to power them from.
- With a PSU present, the flag lets the user (or the application, e.g. a
  panic button) turn the drivers off on demand, independent of PSU state.
- At boot, before the firmware has decided anything, the pin is held HIGH
  (disabled) as the safe default.

The flag is controlled with:

```text
motordriver <id> enable|disable
```

where `<id>` is the PSU element's id (the same id printed by `hardware` and
used with `get <id>`). `get <id>`/`. <id>` on the PSU element reports the flag
as an extra CSV field: `PSU,<ON|OFF>,<voltage>,<motorDriverEnabled 0|1>`.
`test <id> enable` and `steps <id> <n> <speed>` on a motor element both
report `Error: motor driver disabled.` and do nothing while the combined
enable state is off.

The firmware also emits diagnostic telemetry every 500 ms:

```text
VMOTOR: <filtered-voltage>V PSU: <OK|OFF> Motor <n> Dir: <F|R> Position: [rotation <n>, ]<angle> degrees Speed: <rotations-per-second> rps <degrees-per-second> deg/s
```

The firmware also accepts serial commands over the same UART, listed by
sending `help`:

```text
  . [<id>]  Print telemetry, or one element with an id.
  help  Show the list of available commands.
  hardware  List hardware elements by pin, tagged [id].
  get <id>  Print status of one hardware element.
  console enable  Enable periodic console output.
  console disable Disable periodic console output.
  test <id> enable|disable  Toggle test movement for element <id>.
  motordriver <id> enable|disable  Toggle the shared motor driver enable line (PSU element <id>).
  steps <id> <n> <speed>  Move motor <id> by n steps at speed steps/s (blocking; test must be disabled).
```

Every hardware element (each stepper motor, then the power supply detector)
gets a stable numeric id, assigned in the order printed by `hardware`. The
`test` command starts/stops the repeating back-and-forth trapezoid
independently for each motor id, so several motors can be exercised on their
own. `get <id>` and `. <id>` report one element's status as CSV
(`MOTOR,<F|R>,<speed>,<position>,<testEnabled>` or
`PSU,<ON|OFF>,<voltage>,<motorDriverEnabled 0|1>`); `.` with no id prints the
full human-readable telemetry line instead.

`steps <id> <n> <speed>` moves motor `<id>` by exactly `n` microsteps
(positive forward, negative backward, `n != 0`) paced at `speed` microsteps
per second (`speed > 0`), pulsing the driver directly instead of running the
trapezoid profile. Pacing is done with a busy-wait delay
(`StepperMotorDriver::waitMicroseconds`, backed by `avr-libc`'s `_delay_us`)
between pulses rather than the interrupt-driven `SystemClock`: `SystemClock`
only resolves `millis()`, which is far too coarse to space out steps a few
hundred microseconds apart, while `_delay_us` is a cycle-counted loop
calibrated against the compile-time `F_CPU`, so it gives accurate real
microsecond delays as long as `F_CPU` matches the board (which the board
presets already guarantee). This is safe here specifically because the
command is meant to block: there is no background work this firmware needs
to keep running (steps, PSU sampling, telemetry) while a manual jog is in
progress.

The command is blocking end-to-end: the firmware does not read or process
any other command until all `n` steps have been emitted, so it is not
interruptible. It only runs when that motor's `test` is disabled (motor in
SUSTAIN, not running the automatic back-and-forth movement), the motor's
pins are valid, and the external power supply is available; otherwise it
prints an error and does nothing.
