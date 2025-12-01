# TDS-8 v0.30 - Quick Setup Guide

## ⚡ Before You Upload

### 1. GitHub URL Already Set! ✅

**Your GitHub URL is already configured in `OLED30.ino` line 40:**

```cpp
#define GITHUB_MANIFEST_URL "https://raw.githubusercontent.com/m1llipede/tds8/main/manifest.json"
```

**No changes needed!**

### 2. Arduino IDE Settings
- **Board**: XIAO_ESP32C3
- **ESP32 Core**: 2.0.14 (Install via Boards Manager)
- **Partition Scheme**: Minimal SPIFFS (1.9MB APP with OTA)
- **USB CDC On Boot**: Enabled
- **Upload Speed**: 921600

### 3. Install ESP32 Core 2.0.14
1. **Tools → Board → Boards Manager**
2. Search "esp32"
3. Find "esp32 by Espressif Systems"
4. Select version **2.0.14** from dropdown
5. Click Install
6. **Restart Arduino IDE**

---

## 📤 Upload Firmware

### Step 1: Compile & Upload
1. Open `OLED30.ino`
2. Verify settings above
3. Click **Upload** (or Ctrl+U)
4. Wait for "Done uploading"

### Step 2: Test Serial Commands
Open Serial Monitor (115200 baud):
```
VERSION
```
Should respond:
```
VERSION: 0.30
```

### Step 3: Test Track Names
```
/trackname 0 Kick Drum
/trackname 1 Bass Line
/activetrack 0
```

OLEDs should update immediately!

---

## 🌐 Setup GitHub OTA (Optional)

### Create Repository
1. Go to GitHub
2. Create new repository: `tds8-firmware`
3. Make it **public**

### Your manifest.json (Already Created!)
```json
{
  "version": "0.30",
  "url": "https://github.com/m1llipede/tds8/releases/download/v0.30/OLED30.ino.bin"
}
```

### Export Binary
1. **Sketch → Export Compiled Binary**
2. Find `.bin` file in sketch folder
3. Rename to `firmware-0.30.bin`
4. Upload to GitHub repository

### Test OTA
1. Enable WiFi mode:
   ```
   WIFI_JOIN "YourNetwork" "password"
   WIRED_ONLY false
   REBOOT
   ```
2. Go to http://tds8.local
3. Click "Check for Update"
4. Should say "Already up to date"

---

## 🔧 Common Issues

### "Sketch too big" Error
**Fix**: Change Partition Scheme to "Minimal SPIFFS (1.9MB APP with OTA)"

### Serial Commands Don't Work
**Fix**: Check baud rate is 115200, line ending is "Newline"

### Can't Find Board
**Fix**: Install ESP32 Core 2.0.14 via Boards Manager

### WiFi Won't Enable
**Fix**: Mode changes require reboot:
```
WIRED_ONLY false
REBOOT
```

---

## ✅ Verification Checklist

- [ ] Firmware uploads successfully
- [ ] `VERSION` command responds with "0.30"
- [ ] `/trackname` commands update OLEDs
- [ ] `/activetrack` highlights correct screen
- [ ] `/ping` responds with "PONG"
- [ ] Screen 8 shows "USB READY"
- [ ] All 8 OLEDs display track names
- [ ] Desktop bridge can connect
- [ ] (Optional) WiFi mode works
- [ ] (Optional) Web dashboard accessible
- [ ] (Optional) OTA updates work

---

## 🚀 You're Ready!

Your TDS-8 is now running v0.30 with:
- ✅ Wired-first mode (default)
- ✅ Full serial command support
- ✅ Desktop bridge compatibility
- ✅ Optional WiFi mode
- ✅ GitHub OTA updates
- ✅ Colorful web dashboard

**Next**: Open the TDS-8 Desktop Bridge and start creating!
