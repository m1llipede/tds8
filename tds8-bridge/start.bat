@echo off
setlocal

REM ===== Settings =====
set PORT=8088

REM ===== Kill any process listening on PORT =====
for /f "tokens=5" %%P in ('netstat -ano ^| findstr :%PORT% ^| findstr LISTENING') do (
  echo Killing PID %%P using port %PORT%...
  taskkill /PID %%P /F >nul 2>&1
)

REM ===== Launch server from this folder =====
cd /d "%~dp0"

REM optional: small delay so the port frees cleanly
timeout /t 1 /nobreak >nul

REM Open the dashboard (it may 404 for a second until the server is up)
start "" "http://localhost:%PORT%"

REM Start the bridge (leave window open so you can see logs; Ctrl+C to stop)
node index.js

endlocal