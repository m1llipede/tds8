@echo off
echo ============================================
echo   TDS-8 Bridge Debug Launcher
echo ============================================
echo.
echo Killing any existing node processes...
taskkill /F /IM node.exe 2>nul
timeout /t 2 >nul
echo.
echo Starting Bridge backend...
echo.
cd /d "%~dp0"
node index.js
pause
