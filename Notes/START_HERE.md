# 🚀 TDS-8 v0.30 - START HERE

## ✅ What You Have Now

A complete **wired-first** TDS-8 firmware with:
- ✅ USB serial control (default mode)
- ✅ Optional WiFi mode
- ✅ Full bridge compatibility
- ✅ GitHub OTA updates
- ✅ Build identifier to verify version

---

## 📁 Files in This Folder

| File | Purpose |
|------|---------|
| **OLED30.ino** | Main firmware (upload this) |
| **START_HERE.md** | This file - read first! |
| **TESTING_AND_GITHUB_GUIDE.md** | Complete testing steps + GitHub setup |
| **GITHUB_QUICK_START.md** | Visual guide for GitHub |
| **README.md** | Full documentation |
| **SETUP_GUIDE.md** | Quick setup reference |
| **CHANGES.md** | What changed from v26 |

---

## 🎯 Quick Start (3 Steps)

### Step 1: GitHub URL Already Configured! ✅

**Your GitHub URL is already set in `OLED30.ino` line 40:**
```cpp
#define GITHUB_MANIFEST_URL "https://raw.githubusercontent.com/m1llipede/tds8/main/manifest.json"
```

**No changes needed - ready to upload!**

---

### Step 2: Upload Firmware (2 minutes)

**Arduino IDE Settings:**
- Board: **XIAO_ESP32C3**
- ESP32 Core: **2.0.14**
- Partition: **Minimal SPIFFS (1.9MB APP with OTA)**
- USB CDC: **Enabled**

**Click:** Upload button

---

### Step 3: Verify It Works (1 minute)

**Open Serial Monitor (115200 baud)**

**You should see:**
```
=== TDS-8 v0.30 WIRED-FIRST ===
VERSION: 0.30
BUILD: 2025.10.12-wired    ← This confirms new version!
WIRED_ONLY: true
🔌 WIRED MODE - WiFi disabled
✅ Ready for serial commands
```

**Type:** `VERSION`

**You should see:**
```
VERSION: 0.30
BUILD: 2025.10.12-wired
```

**✅ SUCCESS!** You're running the new version!

---

## 🧪 Quick Test

**Type these commands in Serial Monitor:**

```
/trackname 0 Kick Drum
/trackname 1 Snare
/activetrack 0
/ping
```

**Expected:**
- Screen 1 shows "Kick Drum"
- Screen 2 shows "Snare"
- Screen 1 is highlighted (inverted)
- Responds with "PONG"

**✅ If all work, you're good to go!**

---

## 📚 What to Read Next

### For Testing:
👉 **TESTING_AND_GITHUB_GUIDE.md**
- Complete testing checklist
- All serial commands
- WiFi mode testing
- GitHub setup steps

### For GitHub OTA:
👉 **GITHUB_QUICK_START.md**
- Visual guide with examples
- Step-by-step screenshots
- URL formats
- Troubleshooting

### For Reference:
👉 **README.md**
- Full feature documentation
- Comparison tables
- Technical details
- Troubleshooting

---

## 🐙 GitHub Setup (Optional but Recommended)

**Why?** Enables over-the-air firmware updates!

**Time:** 10 minutes

**Steps:**
1. Create `tds8-firmware` repository on GitHub
2. Upload `manifest.json` and `firmware-0.30.bin`
3. Update Arduino code with your URL
4. Test from web dashboard

**Full guide:** See `GITHUB_QUICK_START.md`

---

## 🔄 Switching Between Modes

### Wired Mode (Default)
- WiFi OFF
- Serial commands work
- Lowest latency
- Best for bridge

### WiFi Mode (Optional)
- WiFi ON
- Web dashboard available
- OSC from Ableton
- OTA updates

**To switch:**
```
WIRED_ONLY false
REBOOT
```

**To switch back:**
```
WIRED_ONLY true
REBOOT
```

---

## 🎨 Key Features

### Serial Commands
```
VERSION                     - Show version and build
WIRED_ONLY true/false      - Toggle mode
WIFI_JOIN "ssid" "pass"    - Save WiFi credentials
/trackname <idx> <name>    - Update track name
/activetrack <idx>         - Highlight track
/ping                      - Heartbeat
```

### Display Features
- 8 OLEDs showing track names
- Active track highlighting
- Smart text wrapping
- Status indicators
- USB ready display

### Web Dashboard (WiFi Mode)
- Colorful orange design
- Network configuration
- Tracks editor
- Firmware updates
- WiFi management

---

## 🆘 Troubleshooting

### "Sketch too big" error
**Fix:** Change Partition Scheme to "Minimal SPIFFS"

### Old version shows (no BUILD number)
**Fix:** Make sure you uploaded OLED30.ino, not OLED26

### Serial commands don't work
**Fix:** Check baud rate is 115200, line ending is "Newline"

### Can't find board
**Fix:** Install ESP32 Core 2.0.14 via Boards Manager

---

## 📋 Checklist

Before you're done, verify:

- [ ] Uploaded firmware successfully
- [ ] Serial Monitor shows "BUILD: 2025.10.12-wired"
- [ ] All 8 OLEDs working
- [ ] Screen 8 shows "USB READY"
- [ ] VERSION command shows build number
- [ ] /trackname updates displays
- [ ] /activetrack highlights correctly
- [ ] /ping responds with PONG
- [ ] (Optional) GitHub URL updated
- [ ] (Optional) GitHub repo created
- [ ] (Optional) OTA updates work

---

## 🎯 Next Steps

### Immediate:
1. ✅ Upload firmware
2. ✅ Test serial commands
3. ✅ Verify displays work

### Soon:
1. 🔧 Set up GitHub OTA
2. 🎵 Test with bridge
3. 🎹 Test with Ableton

### Later:
1. 📦 Package for users
2. 📝 Create user guide
3. 🚀 Release to community

---

## 💡 Pro Tips

### Build Identifier
The `BUILD: 2025.10.12-wired` identifier helps you track versions:
- Date: When it was built
- Tag: What feature it has

**For next version, update both:**
```cpp
#define FW_VERSION "0.31"
#define FW_BUILD "2025.10.15-bugfix"
```

### Testing Workflow
1. Make changes
2. Update FW_BUILD with date
3. Upload and test
4. Check build number in serial output
5. Confirms you're testing the right version!

### Version Numbering
- **0.30** - Major.Minor format
- Increment minor for features: 0.31, 0.32
- Increment major for big changes: 1.0

---

## 📞 Need Help?

### Documentation Order:
1. **START_HERE.md** ← You are here
2. **TESTING_AND_GITHUB_GUIDE.md** ← Testing steps
3. **GITHUB_QUICK_START.md** ← GitHub setup
4. **README.md** ← Full reference

### Common Questions:

**Q: How do I know it's the new version?**  
A: Serial Monitor shows "BUILD: 2025.10.12-wired"

**Q: Where do I put firmware files?**  
A: Create GitHub repo called `tds8-firmware`

**Q: What's my GitHub username?**  
A: Check your profile or any repo URL

**Q: Do I need GitHub?**  
A: No, but it enables OTA updates

**Q: Can I use wired and WiFi?**  
A: Yes! Toggle with `WIRED_ONLY` command

---

## 🎉 You're Ready!

Everything is set up and documented. Just:
1. Update GitHub URL (line 40)
2. Upload firmware
3. Test with Serial Monitor
4. Enjoy your wired-first TDS-8!

**Happy coding! 🚀**

---

**Questions? Check the other markdown files for detailed guides!**
