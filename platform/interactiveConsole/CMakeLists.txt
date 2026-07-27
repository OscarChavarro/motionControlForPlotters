cmake_minimum_required(VERSION 3.22)

project(interactiveConsole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Curses REQUIRED)

add_executable(interactiveConsole
  src/main.cpp
  src/io/SerialPort.cpp
  src/gui/ConsoleWidget.cpp
  src/gui/KeyGuideWidget.cpp
  src/gui/InteractiveCommandsInputWidget.cpp
  src/gui/ElementsWidget.cpp
  src/model/CsvLine.cpp
  src/model/PowerSupplyUnitElement.cpp
  src/model/SteperMotorElement.cpp
  src/model/HardwareReportParser.cpp
  src/render/ElementBoxPainter.cpp
  src/render/NcursesPowerSupplyUnitElementRenderer.cpp
  src/render/NcursesSteperMotorElementRenderer.cpp)

target_include_directories(interactiveConsole PRIVATE
  ${CURSES_INCLUDE_DIRS})

target_link_libraries(interactiveConsole PRIVATE
  ${CURSES_LIBRARIES})
