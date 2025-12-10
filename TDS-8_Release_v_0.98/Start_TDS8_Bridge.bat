@echo off
REM =========================================================
REM  TDS-8 Bridge: Universal Launcher
REM  - Kills any stuck Bridge / Node / Electron processes
REM  - Force-clears ports 8088 (HTTP), 9001 (OSC send), 8003 (OSC listen)
REM  - Starts the Electron Bridge application
REM =========================================================

echo ========================================================
echo   TDS-8 BRIDGE LAUNCHER - CLEAN START
echo ========================================================
echo.

REM --- KILL EXISTING PROCESSES ---
echo [1/3] Terminating existing Bridge / Node / Electron processes...
taskkill /F /IM "TDS-8-Bridge.exe" /T >nul 2>nul
taskkill /F /IM "TDS-8 Bridge.exe" /T >nul 2>nul
taskkill /F /IM "node.exe" /T >nul 2>nul
taskkill /F /IM "electron.exe" /T >nul 2>nul
echo     - Any running Bridge / Node / Electron processes have been signaled to close.

echo.
echo [2/3] Hunting specific ports (8088, 9001, 8003)...

REM Kill process on 8088 (HTTP)
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :8088') do (
    echo     - Found PID %%a on port 8088. Killing...
    taskkill /F /PID %%a >nul 2>nul
)

REM Kill process on 9001 (OSC Send)
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :9001') do (
    echo     - Found PID %%a on port 9001. Killing...
    taskkill /F /PID %%a >nul 2>nul
)

REM Kill process on 8003 (OSC Listen)
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :8003') do (
    echo     - Found PID %%a on port 8003. Killing...
    taskkill /F /PID %%a >nul 2>nul
)

echo.
echo [INFO] Verifying ports are free...
timeout /t 2 /nobreak >nul
netstat -ano | findstr ":8088 :9001 :8003" >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo     - SUCCESS: All ports appear free.
) else (
    echo     - WARNING: Some ports are still in use. See above lines for details.
)

echo.
echo [3/3] Preparing Bridge application files...
REM --- BRAIN TRANSPLANT (Silent) ---
cd /d "%~dp0\resources"
if exist "app.asar" ren "app.asar" "app.asar.old" >nul 2>nul
if exist "tds8-bridge" if not exist "app" mklink /J "app" "tds8-bridge" >nul 2>nul

REM --- LAUNCH APPLICATION ---
cd /d "%~dp0"
if exist "TDS-8-Bridge.exe" (
    echo [INFO] Launching TDS-8 Bridge application...
    start "" "TDS-8-Bridge.exe"
) else (
    echo [ERROR] TDS-8-Bridge.exe not found in this folder.
)

echo.
echo [DONE] Bridge launcher finished. This window will now close.
timeout /t 1 /nobreak >nul
exit
