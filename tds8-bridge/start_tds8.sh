#!/bin/bash
# =========================================================
#  TDS-8: Start (Mac/Linux)
#  - Kills old Node processes
#  - Frees ports 8088 (HTTP) and 8000 (OSC) if busy
#  - Starts Node server and opens browser
#  Usage: ./start_tds8.sh
# =========================================================

# Move to script directory
cd "$(dirname "$0")"

# Set ports
export PORT=8088
export OSC_LISTEN_PORT=8000

# Kill any existing Node processes on these ports
echo "[INFO] Stopping existing processes..."
lsof -ti:8088 | xargs kill -9 2>/dev/null
lsof -ti:8000 | xargs kill -9 2>/dev/null

# Wait for ports to be released
sleep 2

# Check if npm is installed
if ! command -v npm &> /dev/null; then
    echo "[ERROR] Node/NPM not found in PATH."
    echo "        Please install Node.js from https://nodejs.org/"
    exit 1
fi

# Start server and open browser
echo "[INFO] Starting TDS-8 Bridge server at http://localhost:$PORT"
echo "[INFO] Opening browser..."
sleep 1

# Open browser (works on Mac and most Linux distros)
if [[ "$OSTYPE" == "darwin"* ]]; then
    open "http://localhost:$PORT"
else
    xdg-open "http://localhost:$PORT" 2>/dev/null || echo "[WARN] Could not auto-open browser. Navigate to http://localhost:$PORT"
fi

# Start the server
npm start
