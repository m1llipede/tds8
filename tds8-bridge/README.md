# TDS-8 Desktop Bridge

**Professional desktop application for TDS-8 OLED display control**

## 🎯 What Is This?

The TDS-8 Bridge is a desktop application that connects your computer to the TDS-8 hardware via USB serial. It provides:

- **Wired-first control** - USB serial connection (default)
- **Optional WiFi mode** - Switch when needed
- **Web-based UI** - Modern, responsive interface
- **Native desktop app** - Mac and Windows support
- **System tray integration** - Runs in background
- **Track name editor** - Control all 8 displays
- **Firmware updates** - OTA update helper

---

## 🚀 Quick Start

### For End Users (Simple!)

1. **Download** the app for your platform:
   - Mac: `TDS-8-0.3.0.dmg`
   - Windows: `TDS-8 Setup 0.3.0.exe`

2. **Install** by dragging to Applications (Mac) or running installer (Windows)

3. **Launch** TDS-8 app

4. **Connect** your TDS-8 via USB-C

5. **Select** serial port from dropdown

6. **Click** "Connect"

7. **Done!** Control your displays!

---

### For Developers

#### Run in Development Mode
```bash
npm install
npm run electron
```

#### Run as CLI (no GUI)
```bash
npm start
```
Then open http://localhost:8088 in browser

#### Build Desktop App
```bash
# Mac
npm run build:mac

# Windows  
npm run build:win

# Both
npm run build:all
```

---

## 📁 Project Structure

```
tds8-bridge/
├── index.js                    # Bridge server (Node.js)
├── electron-main.js            # Electron main process
├── electron-preload.js         # Electron preload script
├── package.json                # Dependencies & build config
├── web/                        # Web UI files
│   ├── index.html
│   ├── app.js
│   └── styles.css
├── ELECTRON_APP_GUIDE.md       # Desktop app documentation
├── BRIDGE_LAUNCHER_GUIDE.md    # Auto-launcher setup
└── README.md                   # This file
```

---

## 🎨 Features

### Wired Mode (Default)
- ✅ USB serial connection
- ✅ No WiFi setup required
- ✅ Lowest latency
- ✅ Most reliable
- ✅ Plug and play

### WiFi Mode (Optional)
- ✅ Wireless control
- ✅ Web dashboard on device
- ✅ OTA firmware updates
- ✅ Network discovery

### Desktop App
- ✅ Native Mac/Windows application
- ✅ System tray integration
- ✅ Auto-start option
- ✅ Professional installer
- ✅ No terminal needed

### Web Interface
- ✅ Track name editor (all 8 displays)
- ✅ Active track selection
- ✅ Serial port selection
- ✅ Connection status
- ✅ Mode switching (wired/WiFi)
- ✅ WiFi network scanner
- ✅ Firmware update helper

---

## 🔌 Connection Modes

### Wired (USB Serial) - Default
```
Computer → USB-C → TDS-8
```
- Fastest
- Most reliable
- No network needed
- Perfect for live performance

### WiFi (Optional)
```
Computer → WiFi → TDS-8
```
- Wireless freedom
- Web dashboard access
- OTA updates
- Good for studio setup

---

## 📖 Documentation

- **ELECTRON_APP_GUIDE.md** - Desktop app setup & building
- **BRIDGE_LAUNCHER_GUIDE.md** - Auto-launcher scripts
- **API.md** - HTTP API documentation (coming soon)

---

## 🛠️ Development

### Prerequisites
- Node.js 18+ (20 recommended)
- npm or yarn
- For building: Mac for .dmg, Windows for .exe

### Install Dependencies
```bash
npm install
```

### Run Development Server
```bash
# CLI mode (browser-based)
npm start

# Desktop app mode
npm run electron

# Desktop app with DevTools
npm run electron-dev
```

### Build for Production
```bash
# Mac only
npm run build:mac

# Windows only
npm run build:win

# Both platforms
npm run build:all
```

**Output:** `dist/` folder with installers

---

## 🎯 Use Cases

### Live Performance
- Wired mode for reliability
- USB connection
- No network dependencies
- Instant response

### Studio Production
- WiFi mode for flexibility
- Web dashboard for monitoring
- OTA updates when needed
- Multiple devices on network

### Development & Testing
- CLI mode for debugging
- Desktop app for user testing
- Easy mode switching
- Serial monitor access

---

## 🔧 Configuration

### Environment Variables
```bash
PORT=8088          # Web server port
BAUD=115200        # Serial baud rate
OTA_MANIFEST_URL=  # Firmware manifest URL
FW_FEED_URL=       # Firmware feed URL
```

### Serial Settings
- **Baud rate:** 115200
- **Data bits:** 8
- **Parity:** None
- **Stop bits:** 1
- **Flow control:** None

---

## 📋 API Endpoints

### Serial Control
- `GET /api/ports` - List available serial ports
- `POST /api/connect` - Connect to serial port
- `POST /api/send` - Send command to device

### Device Control
- `POST /api/trackname` - Update track name
- `POST /api/activetrack` - Set active track
- `POST /api/comm-mode` - Switch wired/WiFi mode

### WiFi Management
- `POST /api/wifi-join` - Join WiFi network
- `GET /api/wifi-scan` - Scan for networks (Windows)

### Firmware Updates
- `GET /api/ota-check` - Check for updates
- `POST /api/ota-update` - Start OTA update
- `GET /api/fw-feed` - Get firmware feed

---

## 🐛 Troubleshooting

### Serial Port Not Found
- Check USB cable (must support data, not just power)
- Try different USB port
- Restart device
- Check drivers (Windows)

### Connection Failed
- Verify baud rate (115200)
- Close other serial monitors
- Check port permissions (Mac)
- Try different port

### App Won't Start
- Check Node.js version: `node --version`
- Reinstall dependencies: `rm -rf node_modules && npm install`
- Check console for errors

### Build Fails
- Clear cache: `rm -rf dist node_modules`
- Reinstall: `npm install`
- Check Electron version compatibility

---

## 📦 Distribution

### For Mac Users
1. Build: `npm run build:mac`
2. Upload `dist/TDS-8-0.3.0.dmg` to GitHub Releases
3. Users download and install

### For Windows Users
1. Build: `npm run build:win` (on Windows or Mac with Wine)
2. Upload `dist/TDS-8 Setup 0.3.0.exe` to GitHub Releases
3. Users download and run installer

### Portable Versions
- Mac: `dist/TDS-8-0.3.0-mac.zip`
- Windows: `dist/TDS-8 0.3.0.exe`

No installation required, just extract and run!

---

## 🎉 Version History

### v0.3.0 (Current)
- ✅ Electron desktop app
- ✅ System tray integration
- ✅ Native installers
- ✅ Wired-first default
- ✅ Auto-start support

### v0.2.0
- ✅ Web-based UI
- ✅ Serial communication
- ✅ WiFi mode support
- ✅ OTA update helper

### v0.1.0
- ✅ Initial CLI version
- ✅ Basic serial relay

---

## 🤝 Contributing

This is part of the TDS-8 project. See main repository for contribution guidelines.

---

## 📄 License

See main TDS-8 repository for license information.

---

## 🙏 Credits

- **Hardware:** XIAO ESP32-C3, SSD1306 OLEDs
- **Software:** Node.js, Electron, Express, SerialPort
- **UI:** Modern web technologies

---

## 🚀 Get Started Now!

```bash
# Clone and install
git clone https://github.com/m1llipede/tds8.git
cd tds8/tds8-bridge
npm install

# Run it!
npm run electron
```

**Enjoy your TDS-8!** 🎵✨
