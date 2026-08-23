cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED ENV{IDF_PATH})
  message(FATAL_ERROR
    "IDF_PATH is not set. Source the ESP-IDF export.sh script before "
    "configuring this firmware, or use scripts/compile.sh.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32ServoBLEController LANGUAGES C CXX)
