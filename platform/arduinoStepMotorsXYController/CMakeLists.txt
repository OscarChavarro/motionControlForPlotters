cmake_minimum_required(VERSION 3.22)

project(arduinoStepMotorsXYController LANGUAGES C CXX)

if(NOT DEFINED MOTION_CONTROL_REPO_ROOT OR NOT DEFINED ARDUINO_BOARD)
  message(FATAL_ERROR
    "This target must be configured as part of the repository root project "
    "(e.g. 'cmake --preset avr-mega2560-debug' from the repo root), which is "
    "the single source of truth for ARDUINO_BOARD and all "
    "STEPPER_MOTOR_*/EXTERNAL_VOLTAGE_PSU_* variables. Configuring this "
    "directory standalone is not supported.")
endif()

include(${MOTION_CONTROL_REPO_ROOT}/motionControlCore/motionControlCoreSources.cmake)

set(ARDUINO_MCU "" CACHE STRING "Override AVR MCU, for example atmega328p")
set(ARDUINO_F_CPU "" CACHE STRING "Override CPU clock, for example 16000000UL")

if(STEPPER_MOTOR_STEP_PIN STREQUAL "0" OR
    STEPPER_MOTOR_STEP_PIN STREQUAL "1" OR
    STEPPER_MOTOR_DIRECTION_PIN STREQUAL "0" OR
    STEPPER_MOTOR_DIRECTION_PIN STREQUAL "1" OR
    STEPPER_MOTOR_Y_STEP_PIN STREQUAL "0" OR
    STEPPER_MOTOR_Y_STEP_PIN STREQUAL "1" OR
    STEPPER_MOTOR_Y_DIRECTION_PIN STREQUAL "0" OR
    STEPPER_MOTOR_Y_DIRECTION_PIN STREQUAL "1" OR
    MOTOR_DRIVER_ENABLE_PIN STREQUAL "0" OR
    MOTOR_DRIVER_ENABLE_PIN STREQUAL "1")
  message(FATAL_ERROR
    "Pins 0 and 1 are not supported while UART serial logging is enabled. "
    "Use distinct digital pins starting at D2.")
endif()

if(MOTOR_DRIVER_ENABLE_PIN STREQUAL STEPPER_MOTOR_STEP_PIN OR
    MOTOR_DRIVER_ENABLE_PIN STREQUAL STEPPER_MOTOR_DIRECTION_PIN OR
    MOTOR_DRIVER_ENABLE_PIN STREQUAL STEPPER_MOTOR_Y_STEP_PIN OR
    MOTOR_DRIVER_ENABLE_PIN STREQUAL STEPPER_MOTOR_Y_DIRECTION_PIN OR
    STEPPER_MOTOR_STEP_PIN STREQUAL STEPPER_MOTOR_DIRECTION_PIN OR
    STEPPER_MOTOR_STEP_PIN STREQUAL STEPPER_MOTOR_Y_STEP_PIN OR
    STEPPER_MOTOR_STEP_PIN STREQUAL STEPPER_MOTOR_Y_DIRECTION_PIN OR
    STEPPER_MOTOR_DIRECTION_PIN STREQUAL STEPPER_MOTOR_Y_STEP_PIN OR
    STEPPER_MOTOR_DIRECTION_PIN STREQUAL STEPPER_MOTOR_Y_DIRECTION_PIN OR
    STEPPER_MOTOR_Y_STEP_PIN STREQUAL STEPPER_MOTOR_Y_DIRECTION_PIN)
  message(FATAL_ERROR
    "The X STEP/DIR, Y STEP/DIR, and MOTOR_DRIVER_ENABLE_PIN pins must all "
    "be distinct.")
endif()

if(NOT STEPPER_MOTOR_Y_DIRECTION_INVERTED STREQUAL "0" AND
    NOT STEPPER_MOTOR_Y_DIRECTION_INVERTED STREQUAL "1")
  message(FATAL_ERROR
    "STEPPER_MOTOR_Y_DIRECTION_INVERTED must be 0 or 1.")
endif()

string(TOLOWER "${ARDUINO_BOARD}" ARDUINO_BOARD_NORMALIZED)

if(ARDUINO_BOARD_NORMALIZED STREQUAL "uno")
  set(_BOARD_MCU "atmega328p")
  set(_BOARD_F_CPU "16000000UL")
  set(_BOARD_PROGRAMMER "arduino")
  set(_BOARD_UPLOAD_BAUD "115200")
  set(_BOARD_MONITOR_BAUD "1000000")
  set(_BOARD_DEFINE "ARDUINO_AVR_UNO")
elseif(ARDUINO_BOARD_NORMALIZED STREQUAL "nano")
  set(_BOARD_MCU "atmega328p")
  set(_BOARD_F_CPU "16000000UL")
  set(_BOARD_PROGRAMMER "arduino")
  set(_BOARD_UPLOAD_BAUD "115200")
  set(_BOARD_MONITOR_BAUD "1000000")
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

execute_process(
  COMMAND date "+%Y_%m%b%d %l:%M%p"
  OUTPUT_VARIABLE MOTION_CONTROL_BUILD_TIMESTAMP
  OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX REPLACE " +" " " MOTION_CONTROL_BUILD_TIMESTAMP
  "${MOTION_CONTROL_BUILD_TIMESTAMP}")
string(TOLOWER "${MOTION_CONTROL_BUILD_TIMESTAMP}"
  MOTION_CONTROL_BUILD_TIMESTAMP)

set(PLATFORM_ARDUINO_SOURCES
  src/AvrExternalPowerSupplyDetector.cpp
  src/AvrGpioLed.cpp
  src/AvrMotorDriverEnable.cpp
  src/AvrStepDirectionDriver.cpp
  src/AvrSystemClock.cpp
  src/AvrUartSerial.cpp
  src/AvrUsartSerial.cpp
  src/SteperMotorDriver.cpp
  src/StepperMotorProgram.cpp
  src/main.cpp
)

add_executable(arduinoStepMotorsXYController
  ${MOTION_CONTROL_CORE_SOURCES}
  ${PLATFORM_ARDUINO_SOURCES})

target_include_directories(arduinoStepMotorsXYController
  PRIVATE
    ${MOTION_CONTROL_REPO_ROOT}/motionControlCore/src/main/cpp
    ${CMAKE_CURRENT_LIST_DIR}/src)

target_compile_features(arduinoStepMotorsXYController PRIVATE cxx_std_17)

target_compile_definitions(arduinoStepMotorsXYController
  PRIVATE
    F_CPU=${_BOARD_F_CPU}
    ARDUINO_SERIAL_BAUD=${_BOARD_MONITOR_BAUD}
    EXTERNAL_VOLTAGE_PSU=${EXTERNAL_VOLTAGE_PSU}
    PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL=${PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL}
    STEPPER_MOTOR_STEP_PIN=${STEPPER_MOTOR_STEP_PIN}
    STEPPER_MOTOR_DIRECTION_PIN=${STEPPER_MOTOR_DIRECTION_PIN}
    STEPPER_MOTOR_Y_STEP_PIN=${STEPPER_MOTOR_Y_STEP_PIN}
    STEPPER_MOTOR_Y_DIRECTION_PIN=${STEPPER_MOTOR_Y_DIRECTION_PIN}
    STEPPER_MOTOR_Y_DIRECTION_INVERTED=${STEPPER_MOTOR_Y_DIRECTION_INVERTED}
    STEPPER_MOTOR_Y_FULL_STEPS_PER_ROTATION=${STEPPER_MOTOR_Y_FULL_STEPS_PER_ROTATION}
    STEPPER_MOTOR_Y_TRAVEL_ROTATIONS=${STEPPER_MOTOR_Y_TRAVEL_ROTATIONS}
    MOTOR_DRIVER_ENABLE_PIN=${MOTOR_DRIVER_ENABLE_PIN}
    STEPPER_MOTOR_STEP_PULSE_MICROSECONDS=${STEPPER_MOTOR_STEP_PULSE_MICROSECONDS}
    STEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS=${STEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS}
    STEPPER_MOTOR_TRAVEL_ROTATIONS=${STEPPER_MOTOR_TRAVEL_ROTATIONS}
    STEPPER_MOTOR_ACCELERATION_MILLISECONDS=${STEPPER_MOTOR_ACCELERATION_MILLISECONDS}
    STEPPER_MOTOR_CRUISE_MILLISECONDS=${STEPPER_MOTOR_CRUISE_MILLISECONDS}
    STEPPER_MOTOR_DECELERATION_MILLISECONDS=${STEPPER_MOTOR_DECELERATION_MILLISECONDS}
    MOTION_CONTROL_BUILD_TIMESTAMP="${MOTION_CONTROL_BUILD_TIMESTAMP}"
    ${_BOARD_DEFINE})

target_compile_options(arduinoStepMotorsXYController
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

target_link_options(arduinoStepMotorsXYController
  PRIVATE
    -mmcu=${_BOARD_MCU}
    -Wl,--gc-sections)

set(MOTIONCONTROL_HEX_FILE
  "${CMAKE_CURRENT_BINARY_DIR}/arduinoStepMotorsXYController.hex")

add_custom_command(
  OUTPUT ${MOTIONCONTROL_HEX_FILE}
  COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom
    $<TARGET_FILE:arduinoStepMotorsXYController> ${MOTIONCONTROL_HEX_FILE}
  DEPENDS arduinoStepMotorsXYController
  COMMENT "Generating Intel HEX firmware")

add_custom_target(arduinoStepMotorsXYControllerHex ALL
  DEPENDS ${MOTIONCONTROL_HEX_FILE})

add_custom_target(arduinoStepMotorsXYControllerSize
  COMMAND ${AVR_SIZE} --mcu=${_BOARD_MCU} --format=avr
    $<TARGET_FILE:arduinoStepMotorsXYController>
  DEPENDS arduinoStepMotorsXYController
  COMMENT "Printing AVR firmware size")

if(AVRDUDE)
  add_custom_target(arduinoStepMotorsXYControllerUpload
    COMMAND ${AVRDUDE}
      -p ${_BOARD_MCU}
      -c ${_BOARD_PROGRAMMER}
      -P ${ARDUINO_PORT}
      -b ${_BOARD_UPLOAD_BAUD}
      -D
      -U flash:w:${MOTIONCONTROL_HEX_FILE}:i
    DEPENDS arduinoStepMotorsXYControllerHex
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
  add_custom_target(arduinoStepMotorsXYControllerMonitor
    COMMAND ${Python3_EXECUTABLE} -m serial.tools.miniterm
      ${ARDUINO_PORT} ${_BOARD_MONITOR_BAUD}
    COMMENT "Opening serial monitor with pyserial miniterm"
    USES_TERMINAL)
elseif(PICOCOM)
  add_custom_target(arduinoStepMotorsXYControllerMonitor
    COMMAND ${PICOCOM} --baud ${_BOARD_MONITOR_BAUD} ${ARDUINO_PORT}
    COMMENT "Opening serial monitor with picocom"
    USES_TERMINAL)
elseif(CMAKE_HOST_UNIX)
  add_custom_target(arduinoStepMotorsXYControllerMonitor
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
