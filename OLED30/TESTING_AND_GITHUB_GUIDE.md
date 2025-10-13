# TDS-8 v0.30 - Testing & GitHub Setup Guide

## 🔍 How to Verify This is the New Version

When you upload and run the firmware, you'll see:

### Serial Monitor Output (on boot):
```
=== TDS-8 v0.30 WIRED-FIRST ===
VERSION: 0.30
BUILD: 2025.10.12-wired
WIRED_ONLY: true
🔌 WIRED MODE - WiFi disabled
💡 Use desktop bridge or send 'WIFI_ON' to enable WiFi
✅ Ready for serial commands
```

### VERSION Command:
Type `VERSION` in Serial Monitor and you'll see:
```
VERSION: 0.30
BUILD: 2025.10.12-wired
```

**The `BUILD: 2025.10.12-wired` confirms this is the new wired-first version!**

---

## 🧪 Complete Testing Guide

### Test 1: Upload & Boot
1. Open `OLED30.ino` in Arduino IDE
2. Set board to **XIAO_ESP32C3**
3. Set partition to **Minimal SPIFFS (1.9MB APP with OTA)**
4. Click **Upload**
5. Open Serial Monitor (115200 baud)
6. **Expected**: See boot message with "BUILD: 2025.10.12-wired"

✅ **Pass**: Shows version and build number  
❌ **Fail**: Shows old version or no build number

---

### Test 2: Wired Mode Default
**Expected Behavior:**
- All 8 OLEDs light up with borders
- Screen 8 (last one) shows "USB READY"
- Screens 1-7 show "Track 1" through "Track 7"
- No WiFi activity (LED should not blink)

✅ **Pass**: All displays working, no WiFi  
❌ **Fail**: WiFi starts, or displays don't show

---

### Test 3: Serial Commands
Open Serial Monitor, test each command:

#### Test VERSION
```
VERSION
```
**Expected:**
```
VERSION: 0.30
BUILD: 2025.10.12-wired
```

#### Test Track Name
```
/trackname 0 Kick Drum
```
**Expected:**
```
OK: /trackname 0 "Kick Drum"
```
**Check**: Screen 1 now shows "Kick Drum"

#### Test Active Track
```
/activetrack 0
```
**Expected:**
```
OK: /activetrack 0
```
**Check**: Screen 1 is now inverted (white background, black text)

#### Test Ping
```
/ping
```
**Expected:**
```
PONG
```
**Check**: Small dot flashes briefly on screen 1

#### Test Multiple Tracks
```
/trackname 0 Kick
/trackname 1 Snare
/trackname 2 Hi-Hat
/trackname 3 Bass
/trackname 4 Synth
/trackname 5 Vocals
/trackname 6 FX
/trackname 7 Master
```
**Expected:** All screens update with new names

#### Test Active Track Switching
```
/activetrack 3
```
**Expected:** Screen 4 (Bass) is now highlighted, screen 1 returns to normal

✅ **Pass**: All commands work, displays update  
❌ **Fail**: Commands error or displays don't update

---

### Test 4: WiFi Mode Toggle
```
WIRED_ONLY false
REBOOT
```
**Expected:**
- Device reboots
- WiFi starts connecting
- Serial shows "📡 WIFI MODE - Starting WiFi..."
- Eventually connects or shows setup instructions

**To return to wired mode:**
```
WIRED_ONLY true
REBOOT
```

✅ **Pass**: Mode switches, WiFi starts/stops  
❌ **Fail**: Mode doesn't change

---

### Test 5: WiFi Credentials (Optional)
```
WIFI_JOIN "YourNetworkName" "YourPassword"
```
**Expected:**
```
OK: WiFi credentials saved (SSID: YourNetworkName)
💡 Set WIRED_ONLY false and reboot to connect
```

Then:
```
WIRED_ONLY false
REBOOT
```
**Expected:** Device connects to your WiFi

✅ **Pass**: Credentials saved, connects after reboot  
❌ **Fail**: Doesn't save or connect

---

### Test 6: Web Dashboard (WiFi Mode Only)
**Prerequisites:** Device in WiFi mode and connected

1. Open browser
2. Go to: `http://tds8.local`
3. **Expected:** Colorful orange dashboard loads

**Check these features:**
- [ ] IP address displays correctly
- [ ] "Check for Update" button exists
- [ ] Tracks editor shows all 8 tracks
- [ ] Can edit track names and save
- [ ] WiFi scan works
- [ ] Firmware version shows "0.30"

✅ **Pass**: Dashboard loads, all features work  
❌ **Fail**: Dashboard doesn't load or features broken

---

## 🐙 GitHub Setup - Step by Step

### Step 1: Create GitHub Repository

1. **Go to GitHub.com** and sign in
2. **Click** the "+" icon (top right) → "New repository"
3. **Repository name**: `tds8-firmware`
4. **Description**: "TDS-8 firmware releases and OTA updates"
5. **Visibility**: **Public** (important for raw file access)
6. **Initialize**: Check "Add a README file"
7. **Click** "Create repository"

**Your repo URL will be:**
```
https://github.com/YOUR_USERNAME/tds8-firmware
```

---

### Step 2: Your manifest.json (Already Set Up!)

**Your manifest.json is already configured in your repo:**
```json
{
  "version": "0.30",
  "url": "https://github.com/m1llipede/tds8/releases/download/v0.30/OLED30.ino.bin"
}
```

**Location**: https://github.com/m1llipede/tds8/blob/main/manifest.json

✅ **Already done!**

---

### Step 3: Export Firmware Binary

1. **In Arduino IDE**, open `OLED30.ino`
2. **Menu**: Sketch → Export Compiled Binary
3. **Wait** for compilation to finish
4. **Find** the `.bin` file in your sketch folder:
   - Mac: `/Users/test/Documents/TDS-8/OLED30/OLED30.ino.bin`
   - Or: Right-click sketch tab → "Show Sketch Folder"
5. **Rename** file to: `firmware-0.30.bin`

---

### Step 4: Upload Binary to GitHub Release

1. **Go to**: https://github.com/m1llipede/tds8/releases
2. **Find** your v0.30 release
3. **Upload** `OLED30.ino.bin` to the Assets section
4. **Publish** the release

**Your release should have:**
```
v0.30 Release Assets:
├── manifest.json
└── OLED30.ino.bin
```

---

### Step 5: Arduino Code Already Updated! ✅

**Your Arduino code is already configured (line 40):**
```cpp
#define GITHUB_MANIFEST_URL "https://raw.githubusercontent.com/m1llipede/tds8/main/manifest.json"
```

**No changes needed - ready to upload!**

---

### Step 6: Test OTA Updates

1. **Put device in WiFi mode**:
   ```
   WIRED_ONLY false
   REBOOT
   ```
2. **Wait** for WiFi to connect
3. **Open browser** → `http://tds8.local`
4. **Scroll** to "Firmware" card
5. **Click** "Check for Update"
6. **Expected**: "Already up to date."

✅ **Success!** OTA is working!

---

### Step 7: Release New Version (Future)

When you make changes and want to release v0.31:

1. **Update** `FW_VERSION` in code:
   ```cpp
   #define FW_VERSION "0.31"
   #define FW_BUILD "2025.10.15-bugfix"
   ```
2. **Export** new binary → rename to `firmware-0.31.bin`
3. **Upload** to GitHub
4. **Update** `manifest.json`:
   ```json
   {
     "version": "0.31",
     "url": "https://raw.githubusercontent.com/YOUR_USERNAME/tds8-firmware/main/firmware-0.31.bin",
     "notes": "Bug fixes and improvements"
   }
   ```
5. **Users** click "Check for Update" → see new version → click to install

---

## 🔗 Your GitHub URLs

### Your GitHub Repository
```
https://github.com/m1llipede/tds8
```

### Your Manifest URL (in Arduino code)
```
https://raw.githubusercontent.com/m1llipede/tds8/main/manifest.json
```

### Your Firmware URL (in manifest.json)
```
https://github.com/m1llipede/tds8/releases/download/v0.30/OLED30.ino.bin
```

### Your Releases Page
```
https://github.com/m1llipede/tds8/releases
```

---

## 📋 Testing Checklist

### Basic Functionality
- [ ] Firmware uploads successfully
- [ ] Serial Monitor shows "BUILD: 2025.10.12-wired"
- [ ] All 8 OLEDs display properly
- [ ] Screen 8 shows "USB READY"
- [ ] VERSION command shows build number

### Serial Commands
- [ ] `/trackname` updates displays
- [ ] `/activetrack` highlights correct screen
- [ ] `/ping` responds with PONG
- [ ] `WIRED_ONLY` toggles mode
- [ ] `WIFI_JOIN` saves credentials
- [ ] `REBOOT` restarts device

### WiFi Mode (Optional)
- [ ] Mode switches successfully
- [ ] Connects to saved network
- [ ] Web dashboard accessible
- [ ] All dashboard features work
- [ ] Can switch back to wired mode

### GitHub OTA (Optional)
- [ ] Repository created
- [ ] manifest.json uploaded
- [ ] firmware-0.30.bin uploaded
- [ ] Arduino code updated with URL
- [ ] "Check for Update" works
- [ ] Shows "Already up to date"

---

## 🆘 Troubleshooting

### "Sketch too big" Error
**Fix:** Change Partition Scheme to "Minimal SPIFFS (1.9MB APP with OTA)"

### Build Number Doesn't Show
**Fix:** Make sure you uploaded the NEW version, not v26

### GitHub 404 Error
**Fix:** 
1. Check repository is **public**
2. Verify username in URL is correct
3. Wait 1-2 minutes after uploading (GitHub caching)

### OTA Update Fails
**Fix:**
1. Device must be in WiFi mode
2. Check manifest.json URL is correct
3. Verify .bin file uploaded correctly
4. Try accessing raw URL directly in browser

---

## ✅ Success Criteria

You'll know everything is working when:

1. ✅ Serial Monitor shows "BUILD: 2025.10.12-wired"
2. ✅ All serial commands work
3. ✅ Displays update in real-time
4. ✅ Can toggle between wired and WiFi modes
5. ✅ Web dashboard loads (WiFi mode)
6. ✅ OTA update check works (WiFi mode)

---

## 🎯 Next Steps

Once testing is complete:
1. Test with your desktop bridge
2. Try with Ableton Live
3. Create more firmware versions
4. Share with users!

**You're all set! 🚀**
