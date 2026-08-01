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
| 2 | D2 / PD2 | D2 / PE4 | Output | A4988 DIR input |
| 3 | D3 / PD3 | D3 / PE5 | Output | A4988 STEP input |
| 13 | D13 / PB5 | D13 / PB7 | Output | Testing LED |
| A0 | ADC0 / PC0 | ADC0 / PF0 | Analog input | External power supply unit detection via voltage divider from VMOT |

The A4988 driver receives one motor step on each rising edge of `STEP`. The `DIR` signal selects the direction used by the next `STEP` rising edge. The firmware uses `D3` for `STEP` and `D2` for `DIR` by default, leaving `D0` and `D1` free for the USB serial console.

`AvrStepDirectionDriver` supports D2-D13 on UNO/Nano and D2-D53 on Mega
2560. The program rejects D0/D1 because this firmware reserves them for UART,
and it rejects repeated pins across registered motors. Consequently, the Mega
has a theoretical maximum of 26 two-pin motor drivers. The current
cooperative, blocking pulse implementation does not yet guarantee that 26
motors can be driven simultaneously at useful rates.

The first stepper motor and its controller are configured with:

```bash
-DSTEPPER_MOTOR_STEP_PIN=3
-DSTEPPER_MOTOR_DIRECTION_PIN=2
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
`GpioLed`, `ExternalPowerSupplyDetector`, and `StepperMotorDriver` interfaces
live in `motionControlCore`. Their concrete implementations in this target are
named `AvrSystemClock`, `AvrUartSerial`, `AvrGpioLed`,
`AvrExternalPowerSupplyDetector`, and `AvrStepDirectionDriver`. `main.cpp`
creates these concrete objects during initialization and uses the core
interface types afterward.

The external power supply detector expects a voltage divider from `VMOT` into `A0`. The divider must keep the analog input inside the board's ADC range.

The default `A0` input circuit is designed to detect the external `VMOT`
input at `24V` while keeping the ADC pin below `5V`:

```text
        220 kOhm
VMOT--/\/\/------ +---- A0
                 |
               47 kOhm
                 |
                GND
```

The default divider values are:

| Resistor | Value | Connection |
|---|---:|---|
| VMOT resistor | 220 KOhm | VMOT to A0 |
| GND resistor | 47 KOhm | A0 to GND |

At `24V`, this divider drives `A0` to about `4.22V`, which stays safely inside
the ADC range.

The detector reconstructs the external `VMOT` voltage from the measured `A0`
voltage and the configured divider values. These CMake variables are passed to
the firmware as compile-time definitions:

| Variable | Default | Used for |
|---|---:|---|
| `EXTERNAL_VOLTAGE_PSU` | `24` | Expected external power supply voltage, in volts |
| `EXTERNAL_VOLTAGE_PSU_TOLERANCE` | `1.5` | Accepted voltage tolerance below `EXTERNAL_VOLTAGE_PSU`, in volts |
| `EXTERNAL_PSU_VOLTAGE_DIVIDER_VIN_RESISTOR_OHMS` | `220000` | Legacy variable name; resistor between `VMOT` and `A0`, in ohms |
| `EXTERNAL_PSU_VOLTAGE_DIVIDER_GND_RESISTOR_OHMS` | `47000` | Resistor between `A0` and `GND`, in ohms |
| `PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL` | `5000` | Minimum interval between missing-PSU error messages, in milliseconds |

The expected PSU voltage is configured at CMake level with:

```bash
-DEXTERNAL_VOLTAGE_PSU=24
```

The detector accepts a voltage tolerance around that value:

```bash
-DEXTERNAL_VOLTAGE_PSU_TOLERANCE=1.5
```

The voltage divider is configured with:

```bash
-DEXTERNAL_PSU_VOLTAGE_DIVIDER_VIN_RESISTOR_OHMS=220000
-DEXTERNAL_PSU_VOLTAGE_DIVIDER_GND_RESISTOR_OHMS=47000
```

The missing-PSU error print interval is configured in milliseconds with:

```bash
-DPSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL=5000
```

At runtime, `AvrExternalPowerSupplyDetector` samples `A0` every 10 ms,
converts the ADC reading to millivolts, and reconstructs the external `VMOT`
voltage using the configured resistor divider. An IIR filter with a coefficient
of `1/8` prevents isolated ADC spikes from restarting motion.

The supply becomes available after 10 consecutive filtered samples at or
above:

```text
EXTERNAL_VOLTAGE_PSU - EXTERNAL_VOLTAGE_PSU_TOLERANCE
```

With the default values, this requires 100 ms at or above `22.5V`. Once
available, the supply is declared lost only after 10 consecutive filtered
samples below `22.1V`. The 400 mV hysteresis prevents repeated transitions
close to the activation threshold.

Confirmed supply transitions produce a concise event containing the filtered
voltage measurement:

```text
EVENT PSU=READY VMOTOR=<voltage>V
EVENT PSU=LOST VMOTOR=<voltage>V
```

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
  steps <id> <n> <speed>  Move motor <id> by n steps at speed steps/s (blocking; test must be disabled).
```

Every hardware element (each stepper motor, then the power supply detector)
gets a stable numeric id, assigned in the order printed by `hardware`. The
`test` command starts/stops the repeating back-and-forth trapezoid
independently for each motor id, so several motors can be exercised on their
own. `get <id>` and `. <id>` report one element's status as CSV
(`MOTOR,<F|R>,<speed>,<position>,<testEnabled>` or `PSU,<ON|OFF>,<voltage>`);
`.` with no id prints the full human-readable telemetry line instead.

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
