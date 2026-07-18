# Instalación y uso del entorno AVR en Linux

Este repositorio usa `CMake` como orquestador y `avr-gcc` para compilación cruzada de firmware AVR escrito en C++17. El flujo genera un `.elf` y un `.hex` para Arduino, sin depender del Arduino IDE.

## Requisitos base

- `cmake` 3.22 o superior
- `avr-gcc`
- `avr-libc`
- `binutils-avr`
- `make` o `ninja`
- `avrdude` para carga por USB

## Paquetes comunes por distribución

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install cmake gcc-avr avr-libc binutils-avr avrdude make
```

### Fedora

```bash
sudo dnf install cmake avr-gcc avr-libc avr-binutils avrdude make
```

### Arch Linux

```bash
sudo pacman -S cmake avr-gcc avr-libc avrdude make
```

## Compilación del firmware Arduino

Desde la raíz del monorepo:

```bash
cmake -S . -B build
cmake --build build --target arduino_hex
```

El `.hex` queda en:

```text
build/platform-arduino/motionControlArduino.hex
```

## Parámetros configurables

La configuración Arduino se controla con variables CMake:

- `ARDUINO_BOARD`: `uno`, `nano`, `mega2560`
- `ARDUINO_PORT`: por ejemplo `/dev/ttyACM0` o `/dev/ttyUSB0`
- `ARDUINO_UPLOAD_BAUD`: velocidad para `avrdude`
- `ARDUINO_MONITOR_BAUD`: velocidad del monitor serial
- `ARDUINO_PROGRAMMER`: programador `avrdude` (`arduino`, `wiring`, etc.)
- `ARDUINO_MCU`: override directo del microcontrolador AVR
- `ARDUINO_F_CPU`: override directo de frecuencia de CPU

Ejemplo:

```bash
cmake -S . -B build \
  -DARDUINO_BOARD=mega2560 \
  -DARDUINO_PORT=/dev/ttyACM0 \
  -DARDUINO_MONITOR_BAUD=115200
cmake --build build --target arduino_hex
```

## Carga por USB con avrdude

Si `avrdude` está instalado:

```bash
cmake --build build/platform-arduino --target motionControlArduinoUpload
```

## Monitor serial por CLI

Si el entorno tiene `pyserial`, el target `motionControlArduinoMonitor` usa:

```bash
python3 -m serial.tools.miniterm <puerto> <baud>
```

Si no, el build intenta usar `picocom` o `screen` si están instalados.

## Permisos de puerto serie

En muchas distribuciones el usuario debe pertenecer a `dialout` o grupo equivalente:

```bash
sudo usermod -aG dialout $USER
```

Después cierra sesión y vuelve a entrar.
