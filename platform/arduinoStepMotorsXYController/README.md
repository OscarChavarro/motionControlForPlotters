# Arduino Step Motors XY Controller

Firmware target for an Arduino UNO or Mega2560-based XY step motor controller.

# Arduino PINOUT

| Pin | UNO | MEGA2560 | Direction | Use |
|---|---|---|---|---|
| 13 | D13 / PB5 | D13 / PB7 | Output | Testing LED |
| A0 | ADC0 / PC0 | ADC0 / PF0 | Analog input | External power supply unit detection via voltage divisor |

The external power supply detector expects a voltage divider from `VIN` into `A0`. The divider must keep the analog input inside the board's ADC range.

The default `A0` input circuit is designed to detect external `VIN` inputs up to 15V while keeping the ADC pin below 5V:

```text
        100 kOhm
VIN ---/\/\/-----+---- A0
                 |
               47 kOhm
                 |
                GND
```

The default divider values are:

| Resistor | Value | Connection |
|---|---:|---|
| VIN resistor | 100 KOhm | VIN to A0 |
| GND resistor | 47 KOhm | A0 to GND |

The detector reconstructs the external `VIN` voltage from the measured `A0` voltage and the configured divider values. These CMake variables are passed to the firmware as compile-time definitions:

| Variable | Default | Used for |
|---|---:|---|
| `EXTERNAL_VOLTAGE_PSU` | `5` | Expected external power supply voltage, in volts |
| `EXTERNAL_VOLTAGE_PSU_TOLERANCE` | `0.1` | Accepted voltage tolerance below `EXTERNAL_VOLTAGE_PSU`, in volts |
| `EXTERNAL_PSU_VOLTAGE_DIVIDER_VIN_RESISTOR_OHMS` | `100000` | Resistor between `VIN` and `A0`, in ohms |
| `EXTERNAL_PSU_VOLTAGE_DIVIDER_GND_RESISTOR_OHMS` | `47000` | Resistor between `A0` and `GND`, in ohms |
| `PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL` | `5000` | Minimum interval between missing-PSU error messages, in milliseconds |

The expected PSU voltage is configured at CMake level with:

```bash
-DEXTERNAL_VOLTAGE_PSU=5
```

The detector accepts a voltage tolerance around that value:

```bash
-DEXTERNAL_VOLTAGE_PSU_TOLERANCE=0.1
```

The voltage divider is configured with:

```bash
-DEXTERNAL_PSU_VOLTAGE_DIVIDER_VIN_RESISTOR_OHMS=100000
-DEXTERNAL_PSU_VOLTAGE_DIVIDER_GND_RESISTOR_OHMS=47000
```

The missing-PSU error print interval is configured in milliseconds with:

```bash
-DPSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL=5000
```

At runtime, `ExternalPowerSupplyDetector` reads `A0`, converts the ADC reading to millivolts, reconstructs the external `VIN` voltage using the configured resistor divider, and compares it against:

```text
EXTERNAL_VOLTAGE_PSU - EXTERNAL_VOLTAGE_PSU_TOLERANCE
```

With the default values, the external power supply is considered present when the reconstructed voltage is at least `4.9V`.

When the external power supply transitions from missing to detected, the firmware prints this message once. This happens at startup if the supply is already connected, and again after each disconnect/reconnect cycle:

```text
External power supply found! Detected voltage: <voltage>V
```

If the external power supply is missing or below the accepted threshold, the firmware prints this error at most once every `PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL` milliseconds:

```text
ERROR: External power supply not found or turned off. Detected voltage: <voltage>V
```

# Serial Commands

The firmware accepts line-based commands over the UART serial port. Send the command text followed by Enter.

| Command | Response |
|---|---|
| `voltage` | Prints the reconstructed external `VIN` voltage from the `A0` detector |
| `verbose off` | Disables periodic `LED=ON` and `LED=OFF` serial messages |
| `verbose on` | Enables periodic `LED=ON` and `LED=OFF` serial messages |

Example response:

```text
Voltage: 5.014V
Verbose: off
Verbose: on
```
