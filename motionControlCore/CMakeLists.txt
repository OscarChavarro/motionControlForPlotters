cmake_minimum_required(VERSION 3.22)

project(motionControlCore LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/motionControlCoreSources.cmake)

add_library(motionControlCore STATIC ${MOTION_CONTROL_CORE_SOURCES})

target_include_directories(motionControlCore
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/main/cpp)

target_compile_features(motionControlCore PUBLIC cxx_std_17)
