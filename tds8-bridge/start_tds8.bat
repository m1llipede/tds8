@echo off
setlocal ENABLEDELAYEDEXPANSION
REM =========================================================
REM  TDS-8: Start (Windows)
REM  - Kills old Electron
REM  - Frees ports 8088 (HTTP) and 8000 (OSC) if busy
REM  - Starts Electron app; falls back to web server
REM  Usage: Double-click this file in the tds8-bridge folder
REM =========================================================

REM --- Move to this script's directory ---
pushd "%~dp0"

REM --- Kill any existing Node/Electron instances ---
echo [INFO] Stopping existing processes...
taskkill /F /IM electron.exe >nul 2>nul
taskkill /F /IM node.exe /FI "WINDOWTITLE eq *tds8*" >nul 2>nul

REM --- Free port 8088 (HTTP) if a process is listening ---
for /f "tokens=5" %%P in ('netstat -ano ^| findstr ":8088"') do (
    echo [INFO] Killing PID %%P on port 8088
    taskkill /PID %%P /F >nul 2>nul
)

REM --- Free port 8000 (OSC) if a process is listening ---
for /f "tokens=5" %%P in ('netstat -ano ^| findstr ":8000"') do (
    echo [INFO] Killing PID %%P on port 8000
    taskkill /PID %%P /F >nul 2>nul
)

REM --- Wait a moment for ports to be released ---
timeout /t 2 /nobreak >nul

REM --- Set the app ports (backup defaults)
set PORT=8088
set OSC_LISTEN_PORT=8000

REM --- Start Node server and open browser ---
where npm >nul 2>nul
if %ERRORLEVEL%==0 (
    echo [INFO] Starting TDS-8 Bridge server at http://localhost:%PORT%
    echo [INFO] Opening browser...
    timeout /t 1 /nobreak >nul
    start http://localhost:%PORT%
    call npm start
    goto end
) else (
    echo [ERROR] Node/NPM not found in PATH.
    echo         Please install Node.js from https://nodejs.org/
    goto end
)

:end
popd
pause
