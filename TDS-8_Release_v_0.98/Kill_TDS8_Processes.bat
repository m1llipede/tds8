@echo off
echo ========================================================
echo   TDS-8 FORCE CLEANUP TOOL
echo ========================================================
echo.

echo [1/4] terminating Electron application...
taskkill /F /IM "TDS-8-Bridge.exe" /T 2>nul
if %ERRORLEVEL% EQU 0 echo    - Killed TDS-8-Bridge.exe
if %ERRORLEVEL% NEQ 0 echo    - TDS-8-Bridge.exe not running

echo.
echo [2/4] Terminating Node.js background processes...
taskkill /F /IM "node.exe" /T 2>nul
if %ERRORLEVEL% EQU 0 echo    - Killed node.exe
if %ERRORLEVEL% NEQ 0 echo    - node.exe not running

echo.
echo [3/4] Hunting specific ports (8088, 9001, 8003)...

REM Kill process on 8088 (HTTP)
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :8088') do (
    echo    - Found PID %%a on port 8088. Killing...
    taskkill /F /PID %%a 2>nul
)

REM Kill process on 9001 (OSC Send)
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :9001') do (
    echo    - Found PID %%a on port 9001. Killing...
    taskkill /F /PID %%a 2>nul
)

REM Kill process on 8003 (OSC Listen)
for /f "tokens=5" %%a in ('netstat -ano ^| findstr :8003') do (
    echo    - Found PID %%a on port 8003. Killing...
    taskkill /F /PID %%a 2>nul
)

echo.
echo [4/4] Verifying Ports...
timeout /t 2 /nobreak >nul
netstat -ano | findstr ":8088 :9001 :8003"
if %ERRORLEVEL% NEQ 0 (
    echo    - SUCCESS: All ports appear free.
) else (
    echo    - WARNING: Some ports are still in use! See above.
)

echo.
echo ========================================================
echo   CLEANUP COMPLETE
echo ========================================================
pause
