cmake_minimum_required(VERSION 3.16)
project(ttplayer-skinsparser VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# QuaZip (ZIP 解压库)
find_package(QuaZip-Qt7 QUIET)
if (NOT QuaZip-Qt7_FOUND)
    find_package(QuaZip QUIET)
endif()
if (NOT QuaZip_FOUND)
    # 尝试手动查找或使用系统路径
    # 用户可能需要设置 QUAZIP_ROOT 或安装 QuaZip
    message(STATUS "QuaZip 未通过 find_package 找到，尝试手动配置...")
    # 如果用户已将 QuaZip 放在特定位置，可以在这里指定 include/lib 路径
endif()

# 头文件路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..   # 项目根目录（可选，用于引用 src/ 下的文件）
)

# 源文件
set(SOURCES
    main.cpp
    skinparser.cpp
)

add_executable(ttplayer-skinsparser ${SOURCES})

# 链接 Qt
find_package(Qt6 COMPONENTS Core QUIET)
if (Qt6_FOUND)
    target_link_libraries(ttplayer-skinsparser PRIVATE Qt6::Core)
else()
    find_package(Qt5 COMPONENTS Core REQUIRED)
    target_link_libraries(ttplayer-skinsparser PRIVATE Qt5::Core)
endif()

# 链接 QuaZip
if (QuaZip-Qt7_FOUND OR QuaZip_FOUND)
    if (QuaZip-Qt7_FOUND)
        target_link_libraries(ttplayer-skinsparser PRIVATE QuaZip-Qt7::QuaZip)
        target_include_directories(ttplayer-skinsparser PRIVATE ${QuaZip-Qt7_INCLUDE_DIRS})
    else()
        target_link_libraries(ttplayer-skinsparser PRIVATE QuaZip::QuaZip)
        target_include_directories(ttplayer-skinsparser PRIVATE ${QuaZip_INCLUDE_DIRS})
    endif()
else()
    message(WARNING "
    ================================================================
     QuaZip 库未找到！skinsparser 需要 QuaZip 来解压 .skn 文件。
     
     请选择以下方式之一:
     
     1) vcpkg: vcpkg install quazip
     2) conda: conda install -c conda-forge quazip
     3) 手动下载: https://github.com/stachenov/quazip/releases
     
     然后设置 CMake 变量:
       cmake -DQUAZIP_ROOT=/path/to/quazip ..
    ================================================================
    ")
endif()

# Windows 下不显示控制台窗口（如果需要的话）
if(WIN32)
    set_target_properties(ttplayer-skinsparser PROPERTIES
        WIN32_EXECUTABLE FALSE   # 保持为控制台程序以便查看输出
        OUTPUT_NAME ttplayer-skinsparser
    )
endif()
