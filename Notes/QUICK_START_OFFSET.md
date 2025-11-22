# TDS-8 Track Offset - Quick Start Guide

## 🚀 Quick Setup

### 1. Upload Arduino Firmware
```bash
# Open Arduino IDE
# File → Open → OLED30/OLED30.ino
# Tools → Board → XIAO ESP32C3
# Tools → Port → [Your TDS-8]
# Click Upload ⬆️
```

### 2. Start Bridge
```bash
cd /Users/test/Music/TDS-8/tds8-bridge
node index.js
```

### 3. Open Web UI
```
http://localhost:8088
```

### 4. Import M4L Patch
```
1. Open Max for Live
2. Create new Audio Effect
3. Click Edit
4. File → Import → Max Text Format
5. Paste contents from tds8_simple_patch.txt
6. Save as "TDS-8 Auto.amxd"
```

---

## 🎮 Using the System

### Web UI Controls:
- **◀ Button**: Previous 8 tracks (offset -= 8)
- **▶ Button**: Next 8 tracks (offset += 8)
- **🔄 Refresh**: Pull track names from Ableton

### Example Workflow:
1. Set offset to **0** → Shows tracks 1-8
2. Click **Refresh** → M4L sends track names
3. OLEDs display track names + numbers
4. Set offset to **8** → Shows tracks 9-16
5. Click **Refresh** → M4L sends new track names

---

## 📋 Command Reference

### Serial Commands:
```bash
/trackname 0 "Kick"           # Display 0 = "Kick" (track 1)
/trackname 0 "Snare" 8        # Display 0 = "Snare" (track 9)
/activetrack 2                # Highlight display 2
/ping                         # Test connection
VERSION                       # Check firmware version
```

### OSC Messages (M4L → Bridge):
```
/trackname <display> <name> <actualTrack>
/activetrack <display>
/refresh <offset>
```

### Expected Flow:
```
M4L → /trackname 0 "Kick" 8 → Bridge (port 8000)
Bridge → /trackname 0 Kick 8 → Arduino (serial)
Arduino → Display "Kick" + "9" → OLED
```

---

## 🧪 Quick Test

### Test 1: Basic Display
```bash
# In Bridge serial monitor or web UI:
/trackname 0 "Test Track"

# Expected on OLED 0:
# Center: "Test Track"
# Bottom: "1"
```

### Test 2: With Offset
```bash
/trackname 0 "Track Nine" 8

# Expected on OLED 0:
# Center: "Track Nine"
# Bottom: "9"
```

### Test 3: All 8 Displays
```bash
/trackname 0 "T9" 8
/trackname 1 "T10" 9
/trackname 2 "T11" 10
/trackname 3 "T12" 11
/trackname 4 "T13" 12
/trackname 5 "T14" 13
/trackname 6 "T15" 14
/trackname 7 "T16" 15

# All displays should show correct numbers at bottom
```

---

## 🎹 M4L Patch Quick Build

### Minimal Working Patch:
```
[udpreceive 9000]  ← Listen for /refresh
|
[route /refresh]
|
[unpack i]  ← Get offset
|
[t i i]
|    \
|     [uzi 8]  ← Loop 8 times
|      |
|      [unpack i i]
|      |
+------[+]  ← Add offset to loop index
       |
    [pack path live_set visible_tracks 0]
       |
    [live.path] → [live.observer] → get name
       |
    [pack i s i]  ← display, name, actual
       |
    [prepend /trackname]
       |
    [udpsend 127.0.0.1 8000]
```

---

## 🐛 Quick Troubleshooting

### No track names showing:
1. ✅ Bridge running? (`node index.js`)
2. ✅ Arduino connected? (Check web UI)
3. ✅ M4L sending to 127.0.0.1:8000?
4. ✅ Check Bridge console for messages

### Wrong track numbers:
1. ✅ Check M4L is sending 3 parameters
2. ✅ Check Bridge logs: `(actual track: X)`
3. ✅ Check Arduino serial: `(track X)`
4. ✅ Remember: Display is 1-based (0→1, 8→9)

### Offset not working:
1. ✅ Click offset buttons in web UI
2. ✅ Check offset display updates
3. ✅ Click Refresh after changing offset
4. ✅ M4L must receive /refresh message

### M4L not responding:
1. ✅ Check M4L is listening on port 9000
2. ✅ Check Bridge is sending to 127.0.0.1:9000
3. ✅ Add `print` objects in Max to debug
4. ✅ Check Max console for errors

---

## 📊 Port Reference

| Component | Receives On | Sends To |
|-----------|-------------|----------|
| M4L | UDP 9000 | UDP 127.0.0.1:8000 |
| Bridge | UDP 8000 | Serial + UDP 127.0.0.1:9000 |
| Arduino | Serial | Serial |
| Web UI | HTTP 8088 | HTTP 8088 |

---

## 🎯 Success Checklist

- [ ] Arduino firmware v0.31 uploaded
- [ ] Bridge running on port 8088
- [ ] Web UI accessible at localhost:8088
- [ ] M4L patch imported and saved
- [ ] M4L device on a track in Ableton
- [ ] Can change offset in web UI
- [ ] Can click refresh button
- [ ] Track names appear on OLEDs
- [ ] Track numbers appear at bottom of OLEDs
- [ ] Active track highlighting works

---

## 📞 Quick Help

### Check Versions:
```bash
# Arduino (in serial monitor):
VERSION
# Expected: VERSION: 0.31

# Bridge (in console):
# Should see: TDS-8 Bridge - Port Configuration
```

### View Logs:
- **Bridge**: Terminal where `node index.js` is running
- **Arduino**: Arduino IDE Serial Monitor (115200 baud)
- **M4L**: Max window console
- **Web UI**: Browser console (F12)

### Reset Everything:
```bash
# 1. Stop Bridge (Ctrl+C)
# 2. Unplug Arduino
# 3. Close Ableton
# 4. Plug in Arduino
# 5. Start Bridge
# 6. Open Ableton
# 7. Test again
```

---

## 🎉 You're Ready!

Everything is set up for track offset support:
- ✅ Arduino displays track numbers
- ✅ Bridge forwards messages
- ✅ Web UI has offset controls
- ⏳ Build M4L patch to complete the system

**Next Step:** Import the M4L patch and test the full workflow!

---

**Last Updated:** October 13, 2025  
**System Version:** Bridge v1.0 + Arduino v0.31
