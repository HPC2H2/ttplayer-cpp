@echo off
echo Building TTPlayer with Qt...

:: Try to find Qt installation
set QT_DIR=
:: Try Qt 6.8.2
if exist "C:\Qt\6.8.2\msvc2019_64" set QT_DIR=C:\Qt\6.8.2\msvc2019_64
if exist "C:\Qt\6.8.2\mingw_64" set QT_DIR=C:\Qt\6.8.2\mingw_64
if exist "D:\Qt\6.8.2\msvc2019_64" set QT_DIR=D:\Qt\6.8.2\msvc2019_64
if exist "D:\Qt\6.8.2\mingw_64" set QT_DIR=D:\Qt\6.8.2\mingw_64

:: Try Qt 6.5.0 if 6.8.2 not found
if "%QT_DIR%"=="" (
    if exist "C:\Qt\6.5.0\msvc2019_64" set QT_DIR=C:\Qt\6.5.0\msvc2019_64
    if exist "C:\Qt\6.5.0\mingw_64" set QT_DIR=C:\Qt\6.5.0\mingw_64
    if exist "D:\Qt\6.5.0\msvc2019_64" set QT_DIR=D:\Qt\6.5.0\msvc2019_64
    if exist "D:\Qt\6.5.0\mingw_64" set QT_DIR=D:\Qt\6.5.0\mingw_64
)

:: Try Qt 6.2.4 if 6.5.0 not found
if "%QT_DIR%"=="" (
    if exist "C:\Qt\6.2.4\msvc2019_64" set QT_DIR=C:\Qt\6.2.4\msvc2019_64
    if exist "C:\Qt\6.2.4\mingw_64" set QT_DIR=C:\Qt\6.2.4\mingw_64
    if exist "D:\Qt\6.2.4\msvc2019_64" set QT_DIR=D:\Qt\6.2.4\msvc2019_64
    if exist "D:\Qt\6.2.4\mingw_64" set QT_DIR=D:\Qt\6.2.4\mingw_64
)

if "%QT_DIR%"=="" (
    echo ERROR: Could not find Qt installation.
    echo Please edit this script to set the correct Qt path.
    goto :end
)

echo Found Qt at: %QT_DIR%

:: Create build directory if it doesn't exist
if not exist build mkdir build
cd build

:: Configure with CMake - using default generator
cmake .. -DCMAKE_PREFIX_PATH=%QT_DIR% -DCMAKE_BINARY_DIR=.

:: Build the project
cmake --build . --config Release

echo.
if %ERRORLEVEL% EQU 0 (
    echo Build completed successfully!
    echo You can find the executable in the build\Release directory.
) else (
    echo Build failed with error code %ERRORLEVEL%
)

:end
cd ..
pause