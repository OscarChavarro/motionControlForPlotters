cmake_minimum_required(VERSION 3.22)

project(motionControl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_subdirectory(motionControlCore)

set(ARDUINO_BOARD "uno" CACHE STRING "Arduino board preset: uno, nano, mega2560")
set(ARDUINO_PORT "/dev/ttyACM0" CACHE STRING "Arduino serial/upload port")
set(ARDUINO_UPLOAD_BAUD "" CACHE STRING "Override upload baud rate")
set(ARDUINO_MONITOR_BAUD "" CACHE STRING "Override monitor baud rate")
set(ARDUINO_PROGRAMMER "" CACHE STRING "Override avrdude programmer")

set(MOTIONCONTROL_ARDUINO_BINARY_DIR "${CMAKE_BINARY_DIR}/platform-arduino")
set(MOTIONCONTROL_ARDUINO_TOOLCHAIN
    "${CMAKE_SOURCE_DIR}/cmake/avr-gcc-toolchain.cmake")

add_custom_target(arduino_configure
  COMMAND
    ${CMAKE_COMMAND}
    -S ${CMAKE_SOURCE_DIR}/platform/arduino
    -B ${MOTIONCONTROL_ARDUINO_BINARY_DIR}
    -DCMAKE_TOOLCHAIN_FILE=${MOTIONCONTROL_ARDUINO_TOOLCHAIN}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DMOTION_CONTROL_REPO_ROOT=${CMAKE_SOURCE_DIR}
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
  COMMAND ${CMAKE_COMMAND} --build ${MOTIONCONTROL_ARDUINO_BINARY_DIR} --target motionControlArduinoHex
  DEPENDS arduino_configure
  COMMENT "Generating Arduino .hex firmware"
  VERBATIM)

add_custom_target(arduino_upload
  COMMAND ${CMAKE_COMMAND} --build ${MOTIONCONTROL_ARDUINO_BINARY_DIR} --target motionControlArduinoUpload
  DEPENDS arduino_configure
  COMMENT "Uploading Arduino firmware"
  VERBATIM)

add_custom_target(arduino_monitor
  COMMAND ${CMAKE_COMMAND} --build ${MOTIONCONTROL_ARDUINO_BINARY_DIR} --target motionControlArduinoMonitor
  DEPENDS arduino_configure
  COMMENT "Opening Arduino serial monitor"
  VERBATIM)
