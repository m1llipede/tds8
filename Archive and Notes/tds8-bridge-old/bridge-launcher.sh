#!/bin/bash

# TDS-8 Bridge Auto-Launcher for Mac
# This script automatically starts the TDS-8 bridge application

# Configuration
BRIDGE_DIR="$(cd "$(dirname "$0")" && pwd)"  # Auto-detect current directory
LOG_FILE="$BRIDGE_DIR/bridge.log"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}🚀 TDS-8 Bridge Launcher${NC}"
echo "================================"

# Check if bridge directory exists
if [ ! -d "$BRIDGE_DIR" ]; then
    echo -e "${RED}❌ Bridge directory not found: $BRIDGE_DIR${NC}"
    echo "Please update BRIDGE_DIR in this script to point to your bridge app location"
    exit 1
fi

# Navigate to bridge directory
cd "$BRIDGE_DIR" || exit 1

# Check if package.json exists
if [ ! -f "package.json" ]; then
    echo -e "${RED}❌ package.json not found. Is this the correct bridge directory?${NC}"
    exit 1
fi

# Check if node_modules exists, if not install dependencies
if [ ! -d "node_modules" ]; then
    echo -e "${YELLOW}📦 Installing dependencies...${NC}"
    npm install
fi

# Start the bridge
echo -e "${GREEN}✅ Starting TDS-8 Bridge...${NC}"
echo "Logs will be written to: $LOG_FILE"
echo ""

# Start bridge and log output
npm start 2>&1 | tee "$LOG_FILE"
