# TDS-8 Electron Desktop App

## 🎉 What's New

Your TDS-8 bridge is now a **native desktop application**!

### Features
- ✅ **Native Mac/Windows app** - No terminal, no Node.js needed for end users
- ✅ **System tray integration** - Runs in background
- ✅ **Auto-start option** - Launch on login
- ✅ **Professional UI** - Native window with your web interface
- ✅ **Wired-first** - Defaults to USB serial connection
- ✅ **Easy distribution** - Single `.app` or `.exe` file

---

## 🚀 Quick Start (Development)

### 1. Install Dependencies
```bash
cd /Users/test/Documents/TDS-8/tds8-bridge
npm install
```

This installs Electron and electron-builder.

### 2. Run in Development Mode
```bash
npm run electron
```

**What happens:**
- Bridge server starts automatically
- Native window opens with your UI
- System tray icon appears
- Ready to connect to TDS-8!

### 3. Test It
1. Window opens automatically
2. Select your TDS-8 serial port
3. Click "Connect"
4. Control your displays!

---

## 📦 Building for Distribution

### Build for Mac
```bash
npm run build:mac
```

**Creates:**
- `dist/TDS-8-0.3.0.dmg` - Installer
- `dist/TDS-8-0.3.0-mac.zip` - Portable app

**Users get:**
- Drag `TDS-8.app` to Applications
- Double-click to launch
- Done!

### Build for Windows
```bash
npm run build:win
```

**Creates:**
- `dist/TDS-8 Setup 0.3.0.exe` - Installer
- `dist/TDS-8 0.3.0.exe` - Portable version

### Build for Both
```bash
npm run build:all
```

---

## 🎨 Customization

### App Icons

You'll need to create icons for the app. I'll help you with that!

**Required files:**
- `web/icon.icns` - Mac icon (512x512 PNG → convert to .icns)
- `web/icon.ico` - Windows icon (256x256 PNG → convert to .ico)
- `web/icon.png` - Window icon (512x512 PNG)
- `web/tray-icon.png` - System tray icon (22x22 PNG for Mac, 16x16 for Windows)

**Quick icon creation:**
1. Create a 512x512 PNG logo
2. Use online converters:
   - PNG → ICNS: https://cloudconvert.com/png-to-icns
   - PNG → ICO: https://cloudconvert.com/png-to-ico
3. Place in `web/` folder

### App Name & Version

Edit `package.json`:
```json
{
  "name": "tds8-bridge",
  "version": "0.3.0",
  "build": {
    "productName": "TDS-8",
    "appId": "com.tds8.bridge"
  }
}
```

---

## 🖥️ System Tray Features

### Mac
- **Icon in menu bar**
- Right-click for menu
- Double-click to show window

### Windows
- **Icon in system tray**
- Right-click for menu
- Double-click to show window
- Minimize to tray

### Tray Menu
- **Show TDS-8** - Open main window
- **Open in Browser** - Open in default browser
- **Quit TDS-8** - Exit app

---

## 🔧 How It Works

### Architecture
```
Electron App
├── Main Process (electron-main.js)
│   ├── Starts bridge server (index.js)
│   ├── Creates native window
│   ├── Manages system tray
│   └── Handles app lifecycle
├── Preload (electron-preload.js)
│   └── Safe API bridge
└── Renderer (your web UI)
    └── Loads from http://localhost:8088
```

### Wired-First Logic
1. App starts → Bridge server starts
2. Window opens → Shows your web UI
3. User selects serial port
4. Connects via USB (wired mode)
5. Optional: Switch to WiFi if needed

---

## 📋 Distribution Checklist

### Before Building
- [ ] Update version in `package.json`
- [ ] Create app icons (icns, ico, png)
- [ ] Test in development mode
- [ ] Test serial connection
- [ ] Test all features

### Build Process
- [ ] Run `npm run build:mac` (on Mac)
- [ ] Run `npm run build:win` (on Windows or Mac with Wine)
- [ ] Test installers on clean machines
- [ ] Create release notes

### Distribution
- [ ] Upload to GitHub Releases
- [ ] Create download page
- [ ] Write installation instructions
- [ ] Share with users!

---

## 🎯 User Experience

### Installation (Mac)
1. Download `TDS-8-0.3.0.dmg`
2. Open DMG
3. Drag `TDS-8.app` to Applications
4. Launch from Applications or Spotlight
5. Grant serial port permissions if prompted

### Installation (Windows)
1. Download `TDS-8 Setup 0.3.0.exe`
2. Run installer
3. Follow wizard
4. Launch from Start Menu or Desktop
5. Grant serial port permissions if prompted

### First Launch
1. App opens automatically
2. Shows "Select Serial Port" dropdown
3. User selects TDS-8 device
4. Clicks "Connect"
5. Displays update in real-time
6. Can minimize to tray

### Daily Use
1. Plug in TDS-8 via USB
2. Launch TDS-8 app (or it's already running in tray)
3. Auto-connects or click "Connect"
4. Control displays
5. Optional: Switch to WiFi mode if needed

---

## 🔍 Troubleshooting

### "npm run electron" doesn't work
```bash
# Install dependencies first
npm install

# Try again
npm run electron
```

### Bridge doesn't start
Check console output:
```bash
# Run with verbose logging
DEBUG=* npm run electron
```

### Serial port permissions (Mac)
Grant permissions in System Preferences:
- Security & Privacy → Privacy → Files and Folders

### Serial port permissions (Windows)
Run as Administrator if needed

### Build fails
```bash
# Clear cache
rm -rf node_modules dist
npm install
npm run build:mac
```

---

## 🚀 Advanced Features

### Auto-Launch on Login

**Mac:**
1. System Preferences → Users & Groups
2. Login Items
3. Add `TDS-8.app`

**Windows:**
1. Press Win+R
2. Type `shell:startup`
3. Create shortcut to `TDS-8.exe`

### Custom Port
Set environment variable:
```bash
PORT=3000 npm run electron
```

### Development Mode
```bash
npm run electron-dev
```
Opens DevTools automatically.

---

## 📝 Next Steps

### Phase 1: Basic App ✅
- [x] Electron wrapper
- [x] System tray
- [x] Native window
- [x] Build scripts

### Phase 2: Enhanced Features
- [ ] Auto-updater (check for new versions)
- [ ] Preferences panel
- [ ] Multiple device support
- [ ] Connection history
- [ ] Custom themes

### Phase 3: M4L Integration
- [ ] Enhanced Max for Live device
- [ ] Auto-detect connection type
- [ ] Fallback logic (WiFi → Serial)
- [ ] Bidirectional communication

---

## 🎉 You're Ready!

Your TDS-8 is now a professional desktop application!

**To test:**
```bash
npm install
npm run electron
```

**To build:**
```bash
npm run build:mac
```

**To distribute:**
Upload `dist/TDS-8-0.3.0.dmg` to GitHub Releases!

---

## 💡 Tips

### For Developers
- Use `npm run electron-dev` for development
- DevTools open automatically
- Hot reload not included (restart to see changes)

### For End Users
- Just download and install
- No Node.js required
- No terminal needed
- Works like any other Mac/Windows app

### For Distribution
- DMG for Mac is easiest
- NSIS installer for Windows is professional
- Portable versions need no installation

---

**Questions? Check the main README or open an issue!** 🚀
