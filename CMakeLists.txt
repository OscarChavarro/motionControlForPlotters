cmake_minimum_required(VERSION 3.22)

project(motionControl LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(MOTION_CONTROL_REPO_ROOT "${CMAKE_SOURCE_DIR}" CACHE PATH
  "Root directory of the motionControl repository")

add_subdirectory(motionControlCore)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "avr")
  set(_MOTION_CONTROL_ENABLE_ARDUINO_IN_TREE_DEFAULT ON)
else()
  set(_MOTION_CONTROL_ENABLE_ARDUINO_IN_TREE_DEFAULT OFF)
endif()

option(MOTION_CONTROL_ENABLE_ARDUINO_IN_TREE
  "Add the Arduino AVR firmware target to this CMake project"
  ${_MOTION_CONTROL_ENABLE_ARDUINO_IN_TREE_DEFAULT})

set(ARDUINO_BOARD "uno" CACHE STRING "Arduino board preset: uno, nano, mega2560")
set(ARDUINO_PORT "/dev/ttyACM0" CACHE STRING "Arduino serial/upload port")
set(ARDUINO_UPLOAD_BAUD "" CACHE STRING "Override upload baud rate")
set(ARDUINO_MONITOR_BAUD "" CACHE STRING "Override monitor baud rate")
set(ARDUINO_PROGRAMMER "" CACHE STRING "Override avrdude programmer")
set(EXTERNAL_VOLTAGE_PSU "24" CACHE STRING
  "Nominal external power supply voltage in volts, reported when the PC817 opto-isolator on A5 detects VMOT present")
set(PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL "5000" CACHE STRING
  "Interval in milliseconds for missing PSU error messages")
set(STEPPER_MOTOR_STEP_PIN "2" CACHE STRING
  "Arduino digital pin connected to the A4988 STEP input")
set(STEPPER_MOTOR_DIRECTION_PIN "5" CACHE STRING
  "Arduino digital pin connected to the A4988 DIR input")
set(STEPPER_MOTOR_Y_STEP_PIN "3" CACHE STRING
  "Arduino digital pin connected to the Y-axis FS31W01 STEP input")
set(STEPPER_MOTOR_Y_DIRECTION_PIN "6" CACHE STRING
  "Arduino digital pin connected to the Y-axis FS31W01 DIR input")
set(STEPPER_MOTOR_Y_DIRECTION_INVERTED "0" CACHE STRING
  "Invert the Y-axis driver DIR signal (0 or 1)")
set(STEPPER_MOTOR_Y_FULL_STEPS_PER_ROTATION "200" CACHE STRING
  "Full steps per rotation of the Y-axis bipolar stepper motor")
set(STEPPER_MOTOR_Y_TRAVEL_ROTATIONS "2" CACHE STRING
  "Rotations traveled by each Y-axis test segment")
set(MOTOR_DRIVER_ENABLE_PIN "8" CACHE STRING
  "Arduino digital pin driving the shared stepper driver EN line (active low)")
set(STEPPER_MOTOR_STEP_PULSE_MICROSECONDS "5" CACHE STRING
  "A4988 STEP high and low pulse width in microseconds")
set(STEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS "5" CACHE STRING
  "Delay between changing DIR and emitting the STEP rising edge")
set(STEPPER_MOTOR_TRAVEL_ROTATIONS "2" CACHE STRING
  "Rotations traveled by each forward or backward movement segment")
set(STEPPER_MOTOR_ACCELERATION_MILLISECONDS "1000" CACHE STRING
  "Acceleration time for the example trapezoid profile")
set(STEPPER_MOTOR_CRUISE_MILLISECONDS "8000" CACHE STRING
  "Cruise time for the example trapezoid profile")
set(STEPPER_MOTOR_DECELERATION_MILLISECONDS "1000" CACHE STRING
  "Deceleration time for the example trapezoid profile")

set(MOTIONCONTROL_ARDUINO_TOOLCHAIN
    "${MOTION_CONTROL_REPO_ROOT}/cmake/avr-gcc-toolchain.cmake")

if(MOTION_CONTROL_ENABLE_ARDUINO_IN_TREE)
  if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "avr")
    message(FATAL_ERROR
      "MOTION_CONTROL_ENABLE_ARDUINO_IN_TREE requires configuring the root "
      "project with the AVR toolchain: "
      "-DCMAKE_TOOLCHAIN_FILE=${MOTIONCONTROL_ARDUINO_TOOLCHAIN}")
  endif()

  add_subdirectory(platform/arduinoStepMotorsXYController
    platform-arduinoStepMotorsXYController)

  add_custom_target(arduino_build
    DEPENDS arduinoStepMotorsXYController)

  add_custom_target(arduino_hex
    DEPENDS arduinoStepMotorsXYControllerHex)

  if(TARGET arduinoStepMotorsXYControllerUpload)
    add_custom_target(arduino_upload
      DEPENDS arduinoStepMotorsXYControllerUpload)
  endif()

  if(TARGET arduinoStepMotorsXYControllerMonitor)
    add_custom_target(arduino_monitor
      DEPENDS arduinoStepMotorsXYControllerMonitor)
  endif()
else()
  set(MOTIONCONTROL_ARDUINO_BINARY_DIR
    "${CMAKE_BINARY_DIR}/platform-arduinoStepMotorsXYController")

  add_custom_target(arduino_configure
    COMMAND
      ${CMAKE_COMMAND}
      -S ${MOTION_CONTROL_REPO_ROOT}/platform/arduinoStepMotorsXYController
      -B ${MOTIONCONTROL_ARDUINO_BINARY_DIR}
      -DCMAKE_TOOLCHAIN_FILE=${MOTIONCONTROL_ARDUINO_TOOLCHAIN}
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DMOTION_CONTROL_REPO_ROOT=${MOTION_CONTROL_REPO_ROOT}
      -DARDUINO_BOARD=${ARDUINO_BOARD}
      -DARDUINO_PORT=${ARDUINO_PORT}
      -DARDUINO_UPLOAD_BAUD=${ARDUINO_UPLOAD_BAUD}
      -DARDUINO_MONITOR_BAUD=${ARDUINO_MONITOR_BAUD}
      -DARDUINO_PROGRAMMER=${ARDUINO_PROGRAMMER}
      -DEXTERNAL_VOLTAGE_PSU=${EXTERNAL_VOLTAGE_PSU}
      -DPSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL=${PSU_NOT_FOUND_ERROR_PRINTING_TIME_INTERVAL}
      -DSTEPPER_MOTOR_STEP_PIN=${STEPPER_MOTOR_STEP_PIN}
      -DSTEPPER_MOTOR_DIRECTION_PIN=${STEPPER_MOTOR_DIRECTION_PIN}
      -DSTEPPER_MOTOR_Y_STEP_PIN=${STEPPER_MOTOR_Y_STEP_PIN}
      -DSTEPPER_MOTOR_Y_DIRECTION_PIN=${STEPPER_MOTOR_Y_DIRECTION_PIN}
      -DSTEPPER_MOTOR_Y_DIRECTION_INVERTED=${STEPPER_MOTOR_Y_DIRECTION_INVERTED}
      -DSTEPPER_MOTOR_Y_FULL_STEPS_PER_ROTATION=${STEPPER_MOTOR_Y_FULL_STEPS_PER_ROTATION}
      -DSTEPPER_MOTOR_Y_TRAVEL_ROTATIONS=${STEPPER_MOTOR_Y_TRAVEL_ROTATIONS}
      -DMOTOR_DRIVER_ENABLE_PIN=${MOTOR_DRIVER_ENABLE_PIN}
      -DSTEPPER_MOTOR_STEP_PULSE_MICROSECONDS=${STEPPER_MOTOR_STEP_PULSE_MICROSECONDS}
      -DSTEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS=${STEPPER_MOTOR_DIRECTION_SETUP_MICROSECONDS}
      -DSTEPPER_MOTOR_TRAVEL_ROTATIONS=${STEPPER_MOTOR_TRAVEL_ROTATIONS}
      -DSTEPPER_MOTOR_ACCELERATION_MILLISECONDS=${STEPPER_MOTOR_ACCELERATION_MILLISECONDS}
      -DSTEPPER_MOTOR_CRUISE_MILLISECONDS=${STEPPER_MOTOR_CRUISE_MILLISECONDS}
      -DSTEPPER_MOTOR_DECELERATION_MILLISECONDS=${STEPPER_MOTOR_DECELERATION_MILLISECONDS}
    COMMENT "Configuring AVR Arduino subproject"
    VERBATIM)

  add_custom_target(arduino_build
    COMMAND ${CMAKE_COMMAND} --build ${MOTIONCONTROL_ARDUINO_BINARY_DIR}
    DEPENDS arduino_configure
    COMMENT "Building AVR Arduino firmware"
    VERBATIM)

  add_custom_target(arduino_hex
    COMMAND ${CMAKE_COMMAND} --build ${MOTIONCONTROL_ARDUINO_BINARY_DIR} --target arduinoStepMotorsXYControllerHex
    DEPENDS arduino_configure
    COMMENT "Generating Arduino .hex firmware"
    VERBATIM)

  add_custom_target(arduino_upload
    COMMAND ${CMAKE_COMMAND} --build ${MOTIONCONTROL_ARDUINO_BINARY_DIR} --target arduinoStepMotorsXYControllerUpload
    DEPENDS arduino_configure
    COMMENT "Uploading Arduino firmware"
    VERBATIM)

  add_custom_target(arduino_monitor
    COMMAND ${CMAKE_COMMAND} --build ${MOTIONCONTROL_ARDUINO_BINARY_DIR} --target arduinoStepMotorsXYControllerMonitor
    DEPENDS arduino_configure
    COMMENT "Opening Arduino serial monitor"
    VERBATIM)
endif()
