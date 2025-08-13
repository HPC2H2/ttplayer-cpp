@echo off
echo Building and testing TTPlayer spectrum display...
cd /d %~dp0
mkdir -p build
cd build
cmake ..
cmake --build .

echo.
echo =============================================
echo Testing TTPlayer with spectrum display fixes:
echo 1. Removed non-linear mapping in MP3 decoder
echo 2. Repositioned spectrum bars to bottom area
echo 3. Enhanced button visibility and clickability
echo =============================================
echo.

start ttplayer.exe
echo Application started. Please check if:
echo - Spectrum bars are now visible at the bottom
echo - Buttons are clickable
echo - Spectrum display is linear (not using non-linear mapping)