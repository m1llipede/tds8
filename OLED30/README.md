# TDS-8 Version 0.30 - Wired-First Edition

## 🎉 What's New in V30

### Consumer-Friendly Wired-First Design
- **Plug & Play**: Device boots in wired mode via USB-C - no WiFi setup required!
- **Works Immediately**: All 8 OLEDs show track names right away
- **Optional WiFi**: Enable WiFi through serial commands or web dashboard when needed
- **Same Features**: Wired mode has identical functionality to WiFi mode

### Key Features

#### 1. **Default Wired Mode** (USB-C)
- Boots with WiFi OFF for instant operation
- All 8 OLEDs display track names immediately
- Screen 8 shows "USB READY" status
- Serial commands work through desktop bridge
- No network configuration needed

#### 2. **Serial Commands** (via Desktop Bridge or Serial Monitor)
```
VERSION                          - Show firmware version
WIRED_ONLY true/false           - Toggle wired/WiFi mode
WIFI_ON                         - Enable WiFi (if credentials saved)
WIFI_JOIN "ssid" "password"     - Save and connect to WiFi
FORGET                          - Clear WiFi credentials
REBOOT                          - Restart device
/trackname <idx> <name>         - Set track name (0-7)
/activetrack <idx>              - Highlight active track
/ping                           - Heartbeat (responds with PONG)
/reannounce                     - Broadcast IP (WiFi mode)
```

#### 3. **WiFi Mode** (Optional)
- Enable via serial command: `WIRED_ONLY false` → `REBOOT`
- Web UI at http://tds8.local
- OSC over UDP for Ableton Live
- OTA firmware updates from GitHub
- Rescue AP if connection fails

#### 4. **GitHub OTA Updates**
- Automatic firmware update checking
- Downloads from your GitHub releases
- One-click update from web dashboard
- Supports versioning and rollback

---

## 📦 Out-of-Box Experience

### Step 1: Plug in USB-C
- OLEDs light up showing borders
- Screen 8 displays "USB READY"
- Device is ready to use!

### Step 2: Open Desktop Bridge
- Bridge auto-detects serial port
- Shows track editor and controls
- Send commands via bridge UI

### Step 3: Use with Ableton (Optional)
- Drag `tds8.amxd` to a track in Ableton
- Track names appear automatically on OLEDs
- Active track highlighting works

### Step 4: Enable WiFi (Optional)
**Via Serial:**
```
WIFI_JOIN "YourNetwork" "password"
WIRED_ONLY false
REBOOT
```

**Via Bridge:**
- Click "Enable WiFi" button
- Enter network credentials
- Toggle mode and reboot

---

## 🔧 Arduino IDE Settings

### Required Settings
- **Board**: XIAO_ESP32C3
- **ESP32 Core**: **2.0.14** (recommended for stability)
- **Partition Scheme**: **Minimal SPIFFS (1.9MB APP with OTA)**
- **USB CDC On Boot**: Enabled
- **Upload Speed**: 921600
- **CPU Frequency**: 160MHz
- **Flash Mode**: QIO
- **Flash Size**: 4MB (32Mb)

### Why These Settings?
- **ESP32 Core 2.0.14**: Most stable WiFi, smaller binaries
- **Minimal SPIFFS Partition**: Gives 1.9MB for code (vs 1.25MB default)
- **USB CDC Enabled**: Required for serial communication

---

## 🌐 GitHub OTA Setup

### 1. Update the Manifest URL
In `OLED30.ino`, line 39:
```cpp
#define GITHUB_MANIFEST_URL "https://raw.githubusercontent.com/YOUR_USERNAME/tds8-firmware/main/manifest.json"
```

Replace `YOUR_USERNAME` with your GitHub username.

### 2. Create GitHub Repository
Create a repository named `tds8-firmware` with this structure:
```
tds8-firmware/
├── manifest.json
├── firmware-0.30.bin
├── firmware-0.31.bin
└── ...
```

### 3. Create manifest.json
```json
{
  "version": "0.30",
  "url": "https://raw.githubusercontent.com/YOUR_USERNAME/tds8-firmware/main/firmware-0.30.bin",
  "notes": "Initial wired-first release"
}
```

### 4. Export Firmware Binary
In Arduino IDE:
1. **Sketch → Export Compiled Binary**
2. Find `.bin` file in sketch folder
3. Rename to `firmware-0.30.bin`
4. Upload to GitHub repository

### 5. Update Process
When releasing new version:
1. Export new binary: `firmware-0.31.bin`
2. Upload to GitHub
3. Update `manifest.json`:
   ```json
   {
     "version": "0.31",
     "url": "https://raw.githubusercontent.com/YOUR_USERNAME/tds8-firmware/main/firmware-0.31.bin",
     "notes": "Bug fixes and improvements"
   }
   ```
4. Users click "Check for Update" in web dashboard
5. Device downloads and installs automatically

---

## 🎨 Web Dashboard

### Features
- **Colorful Orange Design** - Modern gradient headers
- **Network Configuration** - View IP, gateway, DNS
- **WiFi Management** - Scan, join, forget networks
- **Tracks Editor** - Edit all 8 track names at once
- **Firmware Updates** - One-click OTA from GitHub
- **Real-time Status** - Auto-refreshing network info

### Access
- **WiFi Mode**: http://tds8.local or device IP
- **Wired Mode**: Dashboard not available (use bridge instead)

---

## 🔌 Desktop Bridge Integration

### Bridge Expects These Commands
The TDS-8 bridge sends these serial commands:
- `VERSION` - Get firmware version
- `WIRED_ONLY true/false` - Toggle mode
- `WIFI_JOIN "ssid" "pass"` - Configure WiFi
- `/trackname <idx> <name>` - Update track names
- `/activetrack <idx>` - Highlight track
- `/ping` - Keep-alive heartbeat

### Bridge Features
- Serial port auto-detection
- Track name editor (works in wired mode)
- WiFi configuration UI
- Mode switching
- Real-time status display

---

## 📊 Comparison: Wired vs WiFi Mode

| Feature | Wired Mode | WiFi Mode |
|---------|-----------|-----------|
| **Setup Required** | None | WiFi credentials |
| **Boot Time** | Instant | 10-20 seconds |
| **Track Names** | ✅ Via serial | ✅ Via OSC/serial |
| **Active Track** | ✅ Via serial | ✅ Via OSC/serial |
| **Web Dashboard** | ❌ | ✅ |
| **OTA Updates** | ❌ | ✅ |
| **Ableton OSC** | ❌ | ✅ |
| **Desktop Bridge** | ✅ Required | ✅ Optional |
| **Latency** | Lowest | Low |
| **Reliability** | Highest | High |

---

## 🐛 Troubleshooting

### Sketch Too Big Error
**Problem**: `Sketch uses 1317564 bytes (100%) of program storage space`

**Solution**:
1. Change **Partition Scheme** to "Minimal SPIFFS (1.9MB APP with OTA)"
2. Use **ESP32 Core 2.0.14** (not 3.x)
3. Restart Arduino IDE

### Serial Commands Not Working
**Problem**: Device doesn't respond to commands

**Check**:
1. Correct baud rate: **115200**
2. Line ending: **Newline** or **Both NL & CR**
3. USB cable supports data (not power-only)
4. Bridge is connected to correct port

### WiFi Won't Enable
**Problem**: `WIFI_ON` command doesn't work

**Solution**:
```
WIRED_ONLY false
REBOOT
```
Mode change requires reboot to take effect.

### OTA Update Fails
**Problem**: Firmware update doesn't complete

**Check**:
1. Device is in WiFi mode (not wired)
2. GitHub URL is correct and public
3. `.bin` file is valid (exported from Arduino IDE)
4. manifest.json has correct version and URL

---

## 📝 Version History

### v0.30 (Current)
- ✅ Wired-first mode (default WiFi OFF)
- ✅ Full serial command support
- ✅ Desktop bridge compatibility
- ✅ GitHub OTA updates
- ✅ Colorful orange web dashboard
- ✅ Same functionality in wired and WiFi modes
- ✅ Consumer-friendly plug-and-play

### v0.26 (Previous Stable)
- WiFi-first design
- Captive portal setup
- OSC over UDP
- Web dashboard
- OTA updates

---

## 🚀 Quick Start Guide

### For End Users (Wired Mode)
1. Plug in USB-C cable
2. Open TDS-8 Desktop Bridge
3. Bridge auto-connects
4. Start using immediately!

### For Developers (WiFi Mode)
1. Flash firmware with Arduino IDE
2. Send serial commands:
   ```
   WIFI_JOIN "YourNetwork" "password"
   WIRED_ONLY false
   REBOOT
   ```
3. Access web dashboard at http://tds8.local
4. Configure as needed

### For Ableton Users
1. Ensure device is in WiFi mode
2. Drag `tds8.amxd` to a track
3. Track names appear automatically
4. Active track highlights on OLEDs

---

## 📚 Technical Details

### Hardware
- **MCU**: Seeed XIAO ESP32-C3
- **Displays**: 8× SSD1306 OLED (128×64, I²C)
- **Multiplexer**: TCA9548A (I²C, address 0x70)
- **OLED Address**: 0x3C
- **I²C Speed**: 400kHz

### Communication
- **Serial**: 115200 baud, 8N1
- **OSC**: UDP port 8000 (WiFi mode)
- **HTTP**: Port 80 (WiFi mode)
- **mDNS**: tds8.local (WiFi mode)

### Memory Usage
- **Program**: ~1.2MB (fits in 1.9MB partition)
- **RAM**: ~40KB global variables
- **NVS**: Track names, WiFi creds, mode preference

### Power
- **USB-C**: 5V, ~200mA typical
- **OLEDs**: ~10mA each when active
- **WiFi**: +100mA when enabled

---

## 🤝 Contributing

### Reporting Issues
- Use GitHub Issues
- Include firmware version (`VERSION` command)
- Describe expected vs actual behavior
- Include serial output if relevant

### Feature Requests
- Describe use case
- Explain why it's useful
- Consider wired vs WiFi implications

---

## 📄 License

This project is open source. See main repository for license details.

---

## 🙏 Credits

- **Original Design**: v0.26 WiFi-first architecture
- **Wired Mode**: v0.30 consumer-friendly redesign
- **Libraries**: WiFiManager, OSCMessage, Adafruit_SSD1306, ArduinoJson

---

## 📞 Support

- **Documentation**: This README
- **Bridge**: See `tds8-bridge` folder
- **Max for Live**: See `tds8.amxd` device
- **Firmware**: Check GitHub for updates

---

**Enjoy your TDS-8! 🎵**
