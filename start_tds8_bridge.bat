@echo off
REM =========================================================
REM  TDS-8 Bridge: One-Click Launcher
REM  - Kills any old bridge on ports 8088/8090
REM  - Starts Node backend (index.js) on port 8088
REM  - Opens GitHub dashboard in your browser
REM  Usage: Double-click this file from the TDS-8 folder
REM =========================================================

SETLOCAL

REM --- Kill any existing processes on old and new ports ---
echo [INFO] Cleaning up old TDS-8 Bridge processes on ports 8088 and 8090...
for /f "tokens=5" %%A in ('netstat -aon ^| findstr :8088') do @taskkill /F /PID %%A >nul 2>&1
for /f "tokens=5" %%B in ('netstat -aon ^| findstr :8090') do @taskkill /F /PID %%B >nul 2>&1

REM --- Open GitHub-hosted dashboard ---
echo [INFO] Opening TDS-8 Dashboard in your browser...
start "" "https://m1llipede.github.io/tds8"

REM --- Start Node bridge backend on port 8088 ---
echo [INFO] Starting TDS-8 Bridge backend on http://127.0.0.1:8088
pushd "C:\Users\brian\Documents\TDS-8\TDS-8_Bridge\resources\tds8-bridge"
set PORT=8088
node index.js
popd

ENDLOCAL

REM --- Pause so you can see any errors if Node exits ---
pause
