cmake_minimum_required(VERSION 3.16)
project(ESP32Flasher VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6.8.3
set(CMAKE_PREFIX_PATH "/home/cedar/Qt/6.8.3/gcc_64")
find_package(Qt6 REQUIRED COMPONENTS Widgets Core SerialPort)

qt_add_executable(ESP32Flasher
    main.cpp
    mainwindow.h
    mainwindow.cpp
)

target_link_libraries(ESP32Flasher PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::SerialPort
)
