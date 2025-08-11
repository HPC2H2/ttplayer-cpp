cmake_minimum_required(VERSION 3.16)
# Use a project name that matches the current directory to avoid creating a new subdirectory
project(ttplayer-cpp VERSION 1.0.0 LANGUAGES CXX)

# Set build directory to current directory
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler-specific options
if(MSVC)
    add_compile_options(/permissive-)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Try to find Qt6 first, fall back to Qt5 if not found
find_package(Qt6 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets QUIET)
if (NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, trying Qt5...")
    find_package(Qt5 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets REQUIRED)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add resource files
set(RESOURCE_FILES
    resources.qrc
)

# Source files
set(SOURCE_FILES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/playlist.cpp
    src/playlist.h
    src/fadinglabel.cpp
    src/fadinglabel.h
    src/imageslider.cpp
    src/imageslider.h
)

# Create executable
add_executable(TTPlayer ${SOURCE_FILES} ${RESOURCE_FILES})

# Link Qt libraries
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(TTPlayer PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
    )
else()
    target_link_libraries(TTPlayer PRIVATE
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Multimedia
        Qt5::MultimediaWidgets
    )
endif()

# Copy skin files to build directory
file(COPY ${CMAKE_SOURCE_DIR}/skin DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/play_list.txt DESTINATION ${CMAKE_BINARY_DIR})
