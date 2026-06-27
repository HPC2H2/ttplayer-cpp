cmake_minimum_required(VERSION 3.16)
# 同时启用 C 和 C++ 支持
project(ttplayer-cpp VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt
find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
set(QT_HAS_MULTIMEDIA FALSE)

# 查找 zlib（skinparser 需要 raw DEFLATE 解压）
# MinGW 的 zlib 位于 x86_64-w64-mingw32/sysroot 下
set(ZLIB_ROOT "D:/Qt/Tools/mingw1310_64/x86_64-w64-mingw32")
find_package(ZLIB REQUIRED)

if (Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    find_package(Qt6 COMPONENTS Multimedia MultimediaWidgets QUIET)
    if (Qt6Multimedia_FOUND AND Qt6MultimediaWidgets_FOUND)
        set(QT_HAS_MULTIMEDIA TRUE)
        message(STATUS "Qt6 Multimedia components found")
    else()
        message(STATUS "Qt6 Multimedia not found - audio playback disabled")
    endif()
else()
    message(STATUS "Qt6 not found, trying Qt5...")
    find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
    set(QT_VERSION_MAJOR 5)
    find_package(Qt5 COMPONENTS Multimedia MultimediaWidgets QUIET)
    if (Qt5Multimedia_FOUND AND Qt5MultimediaWidgets_FOUND)
        set(QT_HAS_MULTIMEDIA TRUE)
    endif()
endif()

if (QT_HAS_MULTIMEDIA)
    add_compile_definitions(QT_MULTIMEDIA_ENABLED=1)
endif()

# 头文件搜索路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# 资源文件
set(RESOURCE_FILES
    resources.qrc
)

# 源文件
# v2.0 变更:
#   - 移除: src/kiss_fft.c, src/kiss_fftr.c (已用自有 fft.h 替代)
#   - 移除: src/kiss_fft.h, src/kiss_fftr.h, src/_kiss_fft_guts.h (不再需要)
#   - 新增: src/skinengine.cpp, src/skinengine.h (皮肤引擎)
#   - 新增: src/fft.h (自包含 FFT 实现)
#   - 保留: minimp3.h, minimp3_ex.h (header-only MP3 解码器，内嵌使用)
set(SOURCE_FILES
    src/main.cpp
    src/mainwindow.cpp
    src/playlist.cpp
    src/fadinglabel.cpp
    src/imageslider.cpp
    src/spectrumbars.cpp
    src/mp3decoder.cpp
    src/audioplayer.cpp        # ★ Windows 原生音频输出 (waveOut)
    src/skinengine.cpp          # ★ 皮肤引擎
    src/skinparser.cpp          # ★ 皮肤配置解析器（零依赖 ZIP + XML）
)

# 创建可执行文件
add_executable(TTPlayer ${SOURCE_FILES} ${RESOURCE_FILES})

# 链接 Qt 和 zlib
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(TTPlayer PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        ZLIB::ZLIB
        winmm    # Windows Multimedia API (waveOut)
    )
    if(QT_HAS_MULTIMEDIA)
        target_link_libraries(TTPlayer PRIVATE Qt6::Multimedia Qt6::MultimediaWidgets)
    endif()
else()
    target_link_libraries(TTPlayer PRIVATE
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        ZLIB::ZLIB
    )
    if(QT_HAS_MULTIMEDIA)
        target_link_libraries(TTPlayer PRIVATE Qt5::Multimedia Qt5::MultimediaWidgets)
    endif()
endif()

# 复制资源
file(COPY ${CMAKE_SOURCE_DIR}/skin DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/play_list.txt DESTINATION ${CMAKE_BINARY_DIR})

# ============================================================
# 可选构建: skinsparser 命令行工具
# ============================================================
option(BUILD_SKINSPARSER "Build the ttplayer-skinsparser command-line tool" OFF)

if(BUILD_SKINSPARSER)
    add_subdirectory(tools/skinsparser)
endif()
