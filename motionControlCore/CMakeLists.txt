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
endif()
