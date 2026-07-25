cmake_minimum_required(VERSION 3.22)

project(motionControlCore LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/motionControlCoreSources.cmake)

add_library(motionControlCore STATIC ${MOTION_CONTROL_CORE_SOURCES})

target_include_directories(motionControlCore
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/main/cpp)

target_compile_features(motionControlCore PUBLIC cxx_std_17)

if(BUILD_TESTING AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "avr")
  add_executable(motionControlCoreUartSerialTest
    src/test/cpp/hal/UartSerialTest.cpp)
  target_link_libraries(motionControlCoreUartSerialTest
    PRIVATE motionControlCore)
  add_test(
    NAME motionControlCore.UartSerial
    COMMAND motionControlCoreUartSerialTest)

  add_executable(motionControlCoreStepperMotorControllerTest
    src/test/cpp/motion/stepperMotor/StepperMotorControllerTest.cpp)
  target_link_libraries(motionControlCoreStepperMotorControllerTest
    PRIVATE motionControlCore)
  add_test(
    NAME motionControlCore.StepperMotorController
    COMMAND motionControlCoreStepperMotorControllerTest)

  add_executable(motionControlCoreTmc2209CrcTest
    src/test/cpp/drivers/tmc/Tmc2209CrcTest.cpp)
  target_link_libraries(motionControlCoreTmc2209CrcTest
    PRIVATE motionControlCore)
  add_test(
    NAME motionControlCore.Tmc2209Crc
    COMMAND motionControlCoreTmc2209CrcTest)

  add_executable(motionControlCoreTmc2209DatagramTest
    src/test/cpp/drivers/tmc/Tmc2209DatagramTest.cpp)
  target_link_libraries(motionControlCoreTmc2209DatagramTest
    PRIVATE motionControlCore)
  add_test(
    NAME motionControlCore.Tmc2209Datagram
    COMMAND motionControlCoreTmc2209DatagramTest)

  add_executable(motionControlCoreTmc2209RegistersTest
    src/test/cpp/drivers/tmc/Tmc2209RegistersTest.cpp)
  target_link_libraries(motionControlCoreTmc2209RegistersTest
    PRIVATE motionControlCore)
  add_test(
    NAME motionControlCore.Tmc2209Registers
    COMMAND motionControlCoreTmc2209RegistersTest)

  add_executable(motionControlCoreTmc2209UartDriverTest
    src/test/cpp/drivers/tmc/Tmc2209UartDriverTest.cpp)
  target_link_libraries(motionControlCoreTmc2209UartDriverTest
    PRIVATE motionControlCore)
  add_test(
    NAME motionControlCore.Tmc2209UartDriver
    COMMAND motionControlCoreTmc2209UartDriverTest)

  add_executable(motionControlCoreTmc2209StepperMotorDriverTest
    src/test/cpp/drivers/tmc/Tmc2209StepperMotorDriverTest.cpp)
  target_link_libraries(motionControlCoreTmc2209StepperMotorDriverTest
    PRIVATE motionControlCore)
  add_test(
    NAME motionControlCore.Tmc2209StepperMotorDriver
    COMMAND motionControlCoreTmc2209StepperMotorDriverTest)
endif()
