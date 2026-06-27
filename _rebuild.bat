@echo off
setlocal

set QT_DIR=D:\Qt\6.10.3\mingw_64
set CMAKE=D:\Qt\Tools\CMake_64\bin\cmake.exe
set NINJA=D:\Qt\Tools\Ninja\ninja.exe
set MINGW=D:\Qt\Tools\mingw1310_64\bin

set PATH=%MINGW%;%NINJA%;D:\Qt\Tools\CMake_64\bin;%PATH%

if not exist build mkdir build

echo [1/2] CMake Configure (Qt 6.10.3)...
%CMAKE% -S . -B build -G Ninja ^
  -DCMAKE_PREFIX_PATH=%QT_DIR% ^
  -DCMAKE_MAKE_PROGRAM=%NINJA% ^
  -DCMAKE_CXX_COMPILER=%MINGW%\g++.exe ^
  -DCMAKE_C_COMPILER=%MINGW%\gcc.exe
if errorlevel 1 (
    echo === CONFIGURE FAILED ===
    exit /b 1
)

echo.
echo [2/2] Building...
cd build
%ninja%
if errorlevel 1 (
    echo === BUILD FAILED ===
    exit /b 1
)

echo.
echo === BUILD SUCCESS ===
endlocal
