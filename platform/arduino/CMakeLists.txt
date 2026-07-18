cmake_minimum_required(VERSION 3.22)

project(motionControlArduino LANGUAGES C CXX)

if(NOT DEFINED MOTION_CONTROL_REPO_ROOT)
  get_filename_component(MOTION_CONTROL_REPO_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

include(${MOTION_CONTROL_REPO_ROOT}/motionControlCore/motionControlCoreSources.cmake)

set(ARDUINO_BOARD "uno" CACHE STRING "Arduino board preset: uno, nano, mega2560")
set(ARDUINO_MCU "" CACHE STRING "Override AVR MCU, for example atmega328p")
set(ARDUINO_F_CPU "" CACHE STRING "Override CPU clock, for example 16000000UL")
set(ARDUINO_PROGRAMMER "" CACHE STRING "Override avrdude programmer")
set(ARDUINO_PORT "/dev/ttyACM0" CACHE STRING "Upload/monitor serial port")
set(ARDUINO_UPLOAD_BAUD "" CACHE STRING "Override avrdude baud rate")
set(ARDUINO_MONITOR_BAUD "" CACHE STRING "Override monitor serial baud rate")

string(TOLOWER "${ARDUINO_BOARD}" ARDUINO_BOARD_NORMALIZED)

if(ARDUINO_BOARD_NORMALIZED STREQUAL "uno")
  set(_BOARD_MCU "atmega328p")
  set(_BOARD_F_CPU "16000000UL")
  set(_BOARD_PROGRAMMER "arduino")
  set(_BOARD_UPLOAD_BAUD "115200")
  set(_BOARD_MONITOR_BAUD "9600")
  set(_BOARD_DEFINE "ARDUINO_AVR_UNO")
elseif(ARDUINO_BOARD_NORMALIZED STREQUAL "nano")
  set(_BOARD_MCU "atmega328p")
  set(_BOARD_F_CPU "16000000UL")
  set(_BOARD_PROGRAMMER "arduino")
  set(_BOARD_UPLOAD_BAUD "115200")
  set(_BOARD_MONITOR_BAUD "9600")
  set(_BOARD_DEFINE "ARDUINO_AVR_NANO")
elseif(ARDUINO_BOARD_NORMALIZED STREQUAL "mega2560")
  set(_BOARD_MCU "atmega2560")
  set(_BOARD_F_CPU "16000000UL")
  set(_BOARD_PROGRAMMER "wiring")
  set(_BOARD_UPLOAD_BAUD "115200")
  set(_BOARD_MONITOR_BAUD "1000000")
  set(_BOARD_DEFINE "ARDUINO_AVR_MEGA2560")
else()
  message(FATAL_ERROR
    "Unsupported ARDUINO_BOARD='${ARDUINO_BOARD}'. Use uno, nano or mega2560.")
endif()

if(ARDUINO_MCU)
  set(_BOARD_MCU "${ARDUINO_MCU}")
endif()
if(ARDUINO_F_CPU)
  set(_BOARD_F_CPU "${ARDUINO_F_CPU}")
endif()
if(ARDUINO_PROGRAMMER)
  set(_BOARD_PROGRAMMER "${ARDUINO_PROGRAMMER}")
endif()
if(ARDUINO_UPLOAD_BAUD)
  set(_BOARD_UPLOAD_BAUD "${ARDUINO_UPLOAD_BAUD}")
endif()
if(ARDUINO_MONITOR_BAUD)
  set(_BOARD_MONITOR_BAUD "${ARDUINO_MONITOR_BAUD}")
endif()

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

find_program(AVR_OBJCOPY avr-objcopy REQUIRED)
find_program(AVR_SIZE avr-size REQUIRED)
find_program(AVRDUDE avrdude)
find_package(Python3 COMPONENTS Interpreter QUIET)
find_program(PICOCOM picocom)

set(PLATFORM_ARDUINO_SOURCES
  src/main/cpp/main.cpp
  src/main/cpp/platform/arduino/GpioLed.cpp
  src/main/cpp/platform/arduino/SystemClock.cpp
  src/main/cpp/platform/arduino/UartSerial.cpp
)

add_executable(motionControlArduino
  ${MOTION_CONTROL_CORE_SOURCES}
  ${PLATFORM_ARDUINO_SOURCES})

target_include_directories(motionControlArduino
  PRIVATE
    ${MOTION_CONTROL_REPO_ROOT}/motionControlCore/src/main/cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/main/cpp)

target_compile_features(motionControlArduino PRIVATE cxx_std_17)

target_compile_definitions(motionControlArduino
  PRIVATE
    F_CPU=${_BOARD_F_CPU}
    ARDUINO_SERIAL_BAUD=${_BOARD_MONITOR_BAUD}
    ${_BOARD_DEFINE})

target_compile_options(motionControlArduino
  PRIVATE
    -mmcu=${_BOARD_MCU}
    -Wall
    -Wextra
    -Wpedantic
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
    -fno-rtti)

target_link_options(motionControlArduino
  PRIVATE
    -mmcu=${_BOARD_MCU}
    -Wl,--gc-sections)

set(MOTIONCONTROL_HEX_FILE
  "${CMAKE_CURRENT_BINARY_DIR}/motionControlArduino.hex")

add_custom_command(
  OUTPUT ${MOTIONCONTROL_HEX_FILE}
  COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom
    $<TARGET_FILE:motionControlArduino> ${MOTIONCONTROL_HEX_FILE}
  DEPENDS motionControlArduino
  COMMENT "Generating Intel HEX firmware")

add_custom_target(motionControlArduinoHex ALL
  DEPENDS ${MOTIONCONTROL_HEX_FILE})

add_custom_target(motionControlArduinoSize
  COMMAND ${AVR_SIZE} --mcu=${_BOARD_MCU} --format=avr
    $<TARGET_FILE:motionControlArduino>
  DEPENDS motionControlArduino
  COMMENT "Printing AVR firmware size")

if(AVRDUDE)
  add_custom_target(motionControlArduinoUpload
    COMMAND ${AVRDUDE}
      -p ${_BOARD_MCU}
      -c ${_BOARD_PROGRAMMER}
      -P ${ARDUINO_PORT}
      -b ${_BOARD_UPLOAD_BAUD}
      -D
      -U flash:w:${MOTIONCONTROL_HEX_FILE}:i
    DEPENDS motionControlArduinoHex
    COMMENT "Uploading firmware with avrdude")
endif()

if(Python3_Interpreter_FOUND)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import serial"
    RESULT_VARIABLE _PY_SERIAL_RESULT
    OUTPUT_QUIET
    ERROR_QUIET)
else()
  set(_PY_SERIAL_RESULT 1)
endif()

if(_PY_SERIAL_RESULT EQUAL 0)
  add_custom_target(motionControlArduinoMonitor
    COMMAND ${Python3_EXECUTABLE} -m serial.tools.miniterm
      ${ARDUINO_PORT} ${_BOARD_MONITOR_BAUD}
    COMMENT "Opening serial monitor with pyserial miniterm"
    USES_TERMINAL)
elseif(PICOCOM)
  add_custom_target(motionControlArduinoMonitor
    COMMAND ${PICOCOM} --baud ${_BOARD_MONITOR_BAUD} ${ARDUINO_PORT}
    COMMENT "Opening serial monitor with picocom"
    USES_TERMINAL)
elseif(CMAKE_HOST_UNIX)
  add_custom_target(motionControlArduinoMonitor
    COMMAND ${MOTION_CONTROL_REPO_ROOT}/scripts/serial-monitor.sh
      ${ARDUINO_PORT} ${_BOARD_MONITOR_BAUD}
    COMMENT "Opening serial monitor with stty/cat"
    USES_TERMINAL)
endif()

message(STATUS "Arduino build configuration")
message(STATUS "  Board:      ${ARDUINO_BOARD}")
message(STATUS "  MCU:        ${_BOARD_MCU}")
message(STATUS "  F_CPU:      ${_BOARD_F_CPU}")
message(STATUS "  Programmer: ${_BOARD_PROGRAMMER}")
message(STATUS "  Port:       ${ARDUINO_PORT}")
message(STATUS "  Upload:     ${_BOARD_UPLOAD_BAUD}")
message(STATUS "  Monitor:    ${_BOARD_MONITOR_BAUD}")
