#!/bin/bash

# =========================================================
# TDS-8 Bridge: MacOS Launcher
# - Kills any stuck Bridge or Node processes
# - Force-clears ports 8088, 9001, 8003
# - Starts the application
# =========================================================

echo "[TDS-8] Cleaning up environment..."

# --- KILL EXISTING PROCESSES ---
pkill -f "TDS-8 Bridge" 2>/dev/null
pkill -f "node" 2>/dev/null
pkill -f "electron" 2>/dev/null

# --- FORCE CLEAR PORTS ---
# Find and kill process on port 8088
lsof -ti:8088 | xargs kill -9 2>/dev/null
# Find and kill process on port 9001
lsof -ti:9001 | xargs kill -9 2>/dev/null
# Find and kill process on port 8003
lsof -ti:8003 | xargs kill -9 2>/dev/null

# --- LAUNCH APPLICATION ---
echo "[TDS-8] Starting Bridge..."
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Check if the app exists in the current folder, otherwise just try opening
if [ -d "$DIR/TDS-8 Bridge.app" ]; then
    open "$DIR/TDS-8 Bridge.app"
else
    # Fallback: maybe they are running from source or different structure
    echo "Could not find 'TDS-8 Bridge.app' in this folder."
    echo "Trying generic open..."
    # If it's a built app, it might be named differently. 
    # Let's try to find any .app starting with TDS-8
    APP_PATH=$(find "$DIR" -maxdepth 1 -name "TDS-8*.app" | head -n 1)
    if [ -n "$APP_PATH" ]; then
        open "$APP_PATH"
    else
        echo "Error: Could not find TDS-8 application to launch."
        read -p "Press any key to exit..."
    fi
fi

# --- EXIT TERMINAL ---
# This closes the terminal window
osascript -e 'tell application "Terminal" to close first window' & exit
