@echo off
setlocal ENABLEDELAYEDEXPANSION
REM =========================================================
REM  TDS-8: Start (Windows)
REM  - Kills old Electron
REM  - Frees ports 8088 (HTTP), 8000 (OSC), 9000 and 9001 if busy
REM  - Starts server in a new console
REM  - Waits for http://localhost:%PORT% before opening browser
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

REM --- Free port 9000 (OSC to Ableton) if a process is listening ---
for /f "tokens=5" %%P in ('netstat -ano ^| findstr ":9000"') do (
    echo [INFO] Killing PID %%P on port 9000
    taskkill /PID %%P /F >nul 2>nul
)

REM --- Free port 9001 (OSC sender) if a process is listening ---
for /f "tokens=5" %%P in ('netstat -ano ^| findstr ":9001"') do (
    echo [INFO] Killing PID %%P on port 9001
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
    REM Start server in a new console window and keep it open for logs
    start "TDS-8 Bridge Server" cmd /k "npm start"

    echo [INFO] Waiting for server to become ready...
    set /a MAX_TRIES=30
    for /l %%I in (1,1,%MAX_TRIES%) do (
        powershell -NoProfile -Command "try { $r=Invoke-WebRequest -UseBasicParsing http://localhost:%PORT% -TimeoutSec 1; if ($r.StatusCode -ge 200) { exit 0 } } catch { exit 1 }"
        if !ERRORLEVEL! EQU 0 goto openbrowser
        timeout /t 1 /nobreak >nul
    )
    echo [WARN] Server did not respond in expected time; opening browser anyway.
    goto openbrowser
) else (
    echo [ERROR] Node/NPM not found in PATH.
    echo         Please install Node.js from https://nodejs.org/
    goto end
)

:openbrowser
echo [INFO] Opening browser...
start http://localhost:%PORT%

:end
popd
pause
