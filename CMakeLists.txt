cmake_minimum_required(VERSION 3.16)

project(timer-hud LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Needed because main.cpp contains Q_OBJECT classes
set(CMAKE_AUTOMOC ON)

# Prefer Qt6, fall back to Qt5
find_package(Qt6 COMPONENTS Widgets QUIET)

if (Qt6_FOUND)
    set(QT_WIDGETS_LIB Qt6::Widgets)
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)
    set(QT_WIDGETS_LIB Qt5::Widgets)
endif()

# Needed for the global X11 hotkey and X11 window-type override
find_package(X11 REQUIRED)

add_executable(timer-hud main.cpp)

target_link_libraries(timer-hud PRIVATE ${QT_WIDGETS_LIB})

if (TARGET X11::X11)
    target_link_libraries(timer-hud PRIVATE X11::X11)
else()
    target_include_directories(timer-hud PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(timer-hud PRIVATE ${X11_LIBRARIES})
endif()
