cmake_minimum_required(VERSION 3.22)

project(interactiveConsole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Curses REQUIRED)

add_executable(interactiveConsole
  src/main.cpp)

target_include_directories(interactiveConsole PRIVATE
  ${CURSES_INCLUDE_DIRS})

target_link_libraries(interactiveConsole PRIVATE
  ${CURSES_LIBRARIES})
