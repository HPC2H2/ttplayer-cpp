@echo off
echo Building TTPlayer...

if not exist build mkdir build
cd build

cmake .. -G "MinGW Makefiles" -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1310_64/bin/gcc.exe -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    exit /b %ERRORLEVEL%
)

cmake --build .
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo Build completed successfully!
cd ..
