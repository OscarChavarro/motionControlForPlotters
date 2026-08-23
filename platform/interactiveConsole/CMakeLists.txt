cmake_minimum_required(VERSION 3.22)

if(APPLE)
  project(interactiveConsole LANGUAGES CXX OBJCXX)
else()
  project(interactiveConsole LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

add_executable(interactiveConsole
  src/main.cpp
  src/io/SerialPort.cpp
  src/model/ServoElement.cpp
  src/gui/ConsoleWidget.cpp
  src/gui/KeyGuideWidget.cpp
  src/gui/InteractiveCommandsInputWidget.cpp
  src/gui/ElementsWidget.cpp
  src/model/CsvLine.cpp
  src/model/Device.cpp
  src/model/PowerSupplyUnitElement.cpp
  src/model/SteperMotorElement.cpp
  src/model/HardwareReportParser.cpp
  src/render/ElementBoxPainter.cpp
  src/render/NcursesServoElementRenderer.cpp
  src/render/NcursesPowerSupplyUnitElementRenderer.cpp
  src/render/NcursesSteperMotorElementRenderer.cpp)

if(APPLE)
  target_sources(interactiveConsole PRIVATE src/io/BlePortMac.mm)
  target_compile_definitions(interactiveConsole PRIVATE
    MOTION_CONTROL_INTERACTIVE_CONSOLE_COREBLUETOOTH=1)
  target_compile_options(interactiveConsole PRIVATE
    $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  target_link_libraries(interactiveConsole PRIVATE
    "-framework Foundation"
    "-framework CoreBluetooth")
else()
  target_sources(interactiveConsole PRIVATE src/io/BlePortLinux.cpp)
  target_compile_definitions(interactiveConsole PRIVATE
    MOTION_CONTROL_INTERACTIVE_CONSOLE_BLUEZ=1)
endif()

target_include_directories(interactiveConsole PRIVATE
  ${CURSES_INCLUDE_DIRS})

target_link_libraries(interactiveConsole PRIVATE
  ${CURSES_LIBRARIES}
  Threads::Threads)
