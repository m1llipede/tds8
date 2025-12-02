@echo off
REM =========================================================
REM  TDS-8 Bridge: START HERE (One-Click)
REM  - Kills any running TDS-8 Bridge instances
REM  - Starts the desktop Bridge (Electron)
REM  - Opens the dashboard in your browser (optional)
REM =========================================================

setlocal

echo [INFO] Closing any existing TDS-8 Bridge windows...
taskkill /F /IM "TDS-8 Bridge.exe" >nul 2>nul
taskkill /F /IM "TDS-8-Bridge.exe" >nul 2>nul

echo [INFO] Waiting for USB/COM ports to release...
ping 127.0.0.1 -n 2 >nul

echo [INFO] Launching TDS-8 Bridge...
start "TDS-8 Bridge" "%~dp0TDS-8-Bridge.exe"

REM Optional: open local dashboard in default browser if the app window is minimized
ping 127.0.0.1 -n 3 >nul
start http://localhost:8088

endlocal
