cmake_minimum_required(VERSION 3.16)
# 同时启用 C 和 C++ 支持
project(ttplayer-cpp VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt
find_package(Qt6 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets QUIET)
if (NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, trying Qt5...")
    find_package(Qt5 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets REQUIRED)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

# 头文件搜索路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# 资源文件
set(RESOURCE_FILES
    resources.qrc
)

# 源文件
set(SOURCE_FILES
    src/main.cpp
    src/mainwindow.cpp
    src/playlist.cpp
    src/fadinglabel.cpp
    src/imageslider.cpp
    src/spectrumbars.cpp
    src/mp3decoder.cpp
    src/kiss_fft.c
    src/kiss_fftr.c
)

# 创建可执行文件
add_executable(TTPlayer ${SOURCE_FILES} ${RESOURCE_FILES})

# 链接 Qt
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

# 复制资源
file(COPY ${CMAKE_SOURCE_DIR}/skin DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/play_list.txt DESTINATION ${CMAKE_BINARY_DIR})
