# TDS-8 Bridge - Browser-Based Control

Simple, universal web interface for TDS-8 OLED display controller.

## Quick Start

### Windows
```
start_tds8.bat
```
Double-click the batch file or run from PowerShell/Command Prompt.

### Mac/Linux
```bash
chmod +x start_tds8.sh
./start_tds8.sh
```

## What Happens
1. Server starts on `http://localhost:8088`
2. Browser opens automatically
3. Connect your TDS-8 device via USB
4. Control from the web interface

## Requirements
- **Node.js 16+** (Download from https://nodejs.org/)
- **USB connection** to TDS-8 device
- **Modern browser** (Chrome, Firefox, Edge, Safari)

## First Time Setup
```bash
npm install
```

## Features
✅ Works on Windows, Mac, Linux
✅ Access from any device on your network
✅ No app installation needed
✅ Lightweight and fast
✅ Easy updates (just refresh)

## Remote Access (Optional)
To access from other devices on your network:

1. Find your computer's IP address:
   - Windows: `ipconfig`
   - Mac/Linux: `ifconfig` or `ip addr`

2. On another device, navigate to:
   ```
   http://YOUR_IP:8088
   ```
   Example: `http://192.168.1.100:8088`

## Ports Used
- **8088**: HTTP web server
- **8000**: OSC listener (receives from Ableton)
- **9000**: OSC sender (sends to Ableton)
- **9001**: OSC response port

## Troubleshooting

**Server won't start?**
- Check if ports are in use: `netstat -an | findstr 8088`
- Kill the batch file and restart

**Can't connect to device?**
- Check Device Manager (Windows) for COM port
- Try manual COM port connection in the UI
- Click the Refresh button

**Browser doesn't open?**
- Manually navigate to `http://localhost:8088`

**Ableton not connecting?**
- Check M4L device has handshake code
- Look for OSC messages in console logs
- Verify ports 8000/9000 are not blocked

## Development
```bash
# Start with auto-reload
npm run dev

# Check logs
# Look in PowerShell window for all OSC/Serial messages
```

## Cross-Platform Notes
- Uses standard web technologies
- Serial port access via Node.js serialport
- Works identically on all platforms
- No platform-specific builds needed
