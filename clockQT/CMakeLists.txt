cmake_minimum_required(VERSION 3.16)
project(ClockQT VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# 使用 Qt 6.8.3
set(CMAKE_PREFIX_PATH "/home/cedar/Qt/6.8.3/gcc_64")
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core Network)

qt_add_executable(ClockQT
    main.cpp
    clockwidget.h
    clockwidget.cpp
)

target_link_libraries(ClockQT PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core Qt6::Network)

# 复制 logo 到构建目录
file(COPY ${CMAKE_SOURCE_DIR}/墨稿-左右组合.png DESTINATION ${CMAKE_BINARY_DIR})
