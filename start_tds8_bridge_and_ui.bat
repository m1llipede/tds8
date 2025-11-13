@echo off
setlocal

REM ==============================================
REM  TDS-8 Launcher: Start backend then open UI
REM  Place this file in the TDS-8 root folder.
REM  Simple mode: start Node backend from source,
REM  then open GitHub Pages UI in browser.
REM ==============================================

set "SCRIPT_DIR=%~dp0"
set "NODE_EXE=node"
set "NODE_BACKEND=%SCRIPT_DIR%tds8-bridge\index.js"
set "LOG_DIR=%SCRIPT_DIR%logs"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>&1
set "LOG_FILE=%LOG_DIR%\backend.log"

echo [TDS-8] Simple launcher starting...
echo [TDS-8] Backend script: %NODE_BACKEND%

if exist "%NODE_BACKEND%" (
  echo [TDS-8] Starting Node backend in background (logging to %LOG_FILE%)...
  start /min "TDS8-Backend" cmd /c "%NODE_EXE% \"%NODE_BACKEND%\" >> \"%LOG_FILE%\" 2>&1"
) else (
  echo [TDS-8] ERROR: Could not find backend script at:
  echo   %NODE_BACKEND%
  echo [TDS-8] UI will still open, but backend must be started manually.
)

echo [TDS-8] Opening GitHub Pages UI...
start "" "https://m1llipede.github.io/tds8"

endlocal