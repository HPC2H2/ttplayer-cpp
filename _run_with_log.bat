@echo off
chcp 65001 >nul
cd /d "e:\ttplayer-cpp\build"
echo ================================
echo  TTPlayer Debug Log Capture
echo  Start Time: %date% %time%
echo ================================
TTPlayer.exe > "..\debug_log.txt" 2>&1
echo.
echo ================================
echo  Program exited. Log saved to debug_log.txt
echo  End Time: %date% %time%
echo ================================
pause
