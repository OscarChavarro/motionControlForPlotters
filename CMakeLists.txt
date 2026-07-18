cmake_minimum_required(VERSION 3.22)

project(motionControl LANGUAGES CXX)

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
