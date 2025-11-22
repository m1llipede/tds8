@echo off
REM =========================================================
REM  TDS-8 Bridge: Start Server & Kill Old Processes (v2)
REM  Finds and kills any process on port 8088, then starts
REM  the Node.js bridge server and opens the browser.
REM =========================================================

echo [INFO] Killing any existing TDS-8 Bridge executables...
taskkill /F /IM "TDS-8-Bridge.exe" 2>nul
taskkill /F /IM "TDS-8 Bridge.exe" 2>nul

set "PORT=8088"

echo [INFO] Checking for existing processes on port %PORT%...
for /f "tokens=5" %%a in ('netstat -aon ^| findstr :%PORT%') do (
    if "%%a" NEQ "0" (
        echo [INFO] Found process with PID %%a on port %PORT%. Terminating...
        taskkill /F /PID %%a
    ) else (
        echo [INFO] No active process found on port %PORT%.
    )
)

echo [INFO] Starting TDS-8 Bridge server...
start "TDS-8 Bridge" node index.js

echo [INFO] Waiting for server to initialize...
timeout /t 3 /nobreak >nul

echo [INFO] Opening bridge dashboard at http://localhost:%PORT%
explorer "http://localhost:%PORT%"

echo [SUCCESS] Bridge server started and browser opened.
echo.
echo This window will remain open. You can close it when you're done.

