#!/bin/bash
set -euo pipefail

# TDS-8 Bridge quick launcher for macOS
# - Closes any existing instances
# - Launches the app from /Applications
# - Opens the dashboard in your browser

echo "[INFO] Closing any existing TDS-8 Bridge..."
pkill -f "TDS-8-Bridge" >/dev/null 2>&1 || true
sleep 0.5

APP_NAME="TDS-8-Bridge"

echo "[INFO] Launching ${APP_NAME}..."
if ! open -a "${APP_NAME}" ; then
  echo "[WARN] ${APP_NAME} not found in /Applications."
  echo "[HINT] If you opened the DMG, drag ${APP_NAME}.app into Applications first, then run this again."
  exit 1
fi

# Give the bridge time to boot the local server
sleep 2

# Open the dashboard
open "http://localhost:8088"

echo "[SUCCESS] ${APP_NAME} started. This window can be closed."
