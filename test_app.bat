@echo off
echo Building and running TTPlayer...
cd /d %~dp0
mkdir -p build
cd build
cmake ..
cmake --build .
start ttplayer.exe
echo Application started.