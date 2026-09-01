cmake_minimum_required(VERSION 3.20)
project(gsr_focused_tray VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Widgets)
find_package(KF6StatusNotifierItem REQUIRED)

add_executable(gsr-focused-tray main.cpp)
target_link_libraries(gsr-focused-tray PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Widgets
    KF6::StatusNotifierItem
)

install(TARGETS gsr-focused-tray RUNTIME DESTINATION bin)
