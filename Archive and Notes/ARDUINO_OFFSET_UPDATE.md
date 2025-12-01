# Arduino Firmware Update - Track Offset Support (v0.31)

## 🎯 Changes Made

Updated the TDS-8 Arduino firmware to support track offset functionality, allowing each OLED to display both the track name and the actual track number.

---

## 📝 Summary of Changes

### 1. **Added Track Number Storage** (Line 57)
```cpp
int actualTrackNumbers[numScreens] = {0, 1, 2, 3, 4, 5, 6, 7};
```
- New array to store the actual track number for each display
- Defaults to 0-7 (matching display indices)

### 2. **Updated Serial Command Handler** (Lines 1404-1450)
**Old format:** `/trackname <idx> <name>`  
**New format:** `/trackname <idx> <name> [actualTrack]`

The command now accepts an optional 3rd parameter:
- `idx` - Display index (0-7)
- `name` - Track name to display
- `actualTrack` - Optional actual track number (defaults to idx if not provided)

**Example commands:**
```
/trackname 0 "Kick" 0        → Display 0 shows "Kick" (track 1)
/trackname 0 "Snare" 8       → Display 0 shows "Snare" (track 9)
/trackname 1 "Hi-Hat" 9      → Display 1 shows "Hi-Hat" (track 10)
```

### 3. **Updated OSC Handler** (Lines 720-746)
The WiFi OSC handler now also accepts the optional 3rd parameter for consistency.

### 4. **Enhanced Display Function** (Lines 562-571)
Added code to draw the actual track number at the bottom of each OLED:

```cpp
// Draw actual track number at bottom (small text)
display.setTextSize(1);
String trackNumStr = String(actualTrackNumbers[screen] + 1); // +1 for 1-based display
int16_t x1, y1;
uint16_t w, h;
display.getTextBounds(trackNumStr, 0, 0, &x1, &y1, &w, &h);
int trackNumX = (SCREEN_WIDTH - w) / 2;
int trackNumY = SCREEN_HEIGHT - h - 3; // 3 pixels from bottom
display.setCursor(trackNumX, trackNumY);
display.println(trackNumStr);
```

**Visual Layout:**
```
┌──────────────┐
│              │
│    Kick      │  ← Track name (center, large)
│              │
│      9       │  ← Actual track number (bottom, small)
└──────────────┘
```

### 5. **Updated Help Text** (Line 1520)
```
/trackname <idx> <name> [actualTrack] - Set track name (0-7)
```

### 6. **Version Bump** (Lines 38-39)
- Version: `0.30` → `0.31`
- Build: `2025.10.12-wired` → `2025.10.13-offset`

---

## 🔄 Backward Compatibility

✅ **Fully backward compatible!**

The 3rd parameter is optional, so existing code will continue to work:
- Old format: `/trackname 0 "Kick"` → Works (actualTrack defaults to 0)
- New format: `/trackname 0 "Kick" 8` → Works (actualTrack = 8)

---

## 🧪 Testing

### Test 1: Basic Command (No Offset)
```bash
/trackname 0 "Kick"
```
**Expected:**
- Display 0 shows "Kick" in center
- Display 0 shows "1" at bottom (actualTrack defaults to 0, displayed as 1)

### Test 2: With Offset
```bash
/trackname 0 "Snare" 8
```
**Expected:**
- Display 0 shows "Snare" in center
- Display 0 shows "9" at bottom (actualTrack = 8, displayed as 9)

### Test 3: Full Offset Range
```bash
/trackname 0 "Track 9" 8
/trackname 1 "Track 10" 9
/trackname 2 "Track 11" 10
/trackname 3 "Track 12" 11
/trackname 4 "Track 13" 12
/trackname 5 "Track 14" 13
/trackname 6 "Track 15" 14
/trackname 7 "Track 16" 15
```
**Expected:**
- All 8 displays show correct track names
- Bottom numbers show 9, 10, 11, 12, 13, 14, 15, 16

---

## 📦 How It Integrates

### Bridge → Arduino Flow:
1. **M4L** sends: `/trackname 0 "Kick" 8` to Bridge (port 8000)
2. **Bridge** forwards to Arduino via serial: `/trackname 0 Kick 8\n`
3. **Arduino** parses command:
   - `idx = 0`
   - `name = "Kick"`
   - `actualTrack = 8`
4. **Arduino** updates:
   - `trackNames[0] = "Kick"`
   - `actualTrackNumbers[0] = 8`
5. **Arduino** draws display:
   - Center: "Kick" (large text)
   - Bottom: "9" (small text, 8+1 for 1-based)

---

## 🎨 Display Layout Details

### Text Positioning:
- **Track Name**: Centered vertically and horizontally
  - Uses existing smart layout (1-2 lines, auto-sizing)
  - Text size: 1 or 2 (auto-selected)
  
- **Track Number**: Bottom center
  - Text size: 1 (small)
  - Position: 3 pixels from bottom edge
  - Centered horizontally
  - 1-based numbering (0 → "1", 8 → "9", etc.)

### Visual Hierarchy:
```
┌────────────────┐
│  ┌──────────┐  │  ← Border (existing)
│  │          │  │
│  │  Track   │  │  ← Track name (large, centered)
│  │   Name   │  │
│  │          │  │
│  │    9     │  │  ← Track number (small, bottom)
│  └──────────┘  │
└────────────────┘
```

---

## 🚀 Upload Instructions

### Using Arduino IDE:
1. Open `OLED30.ino` in Arduino IDE
2. Select board: **XIAO ESP32C3**
3. Select port: Your TDS-8 device
4. Click **Upload**
5. Wait for upload to complete
6. Open Serial Monitor (115200 baud)
7. Should see: `VERSION: 0.31`

### Using PlatformIO:
```bash
cd /Users/test/Music/TDS-8/OLED30
pio run -t upload
pio device monitor
```

### Verify Upload:
```bash
# In serial monitor, type:
VERSION

# Expected response:
VERSION: 0.31
BUILD: 2025.10.13-offset
```

---

## 🔍 Troubleshooting

### Track number not showing:
- Check that display is being refreshed
- Verify `actualTrackNumbers` array is populated
- Check serial output for confirmation

### Wrong track number displayed:
- Verify 3rd parameter is being sent correctly
- Check serial output: `OK: /trackname 0 "Kick" (track 8)`
- Remember: Display is 1-based (0 → "1", 8 → "9")

### Track name overlaps with number:
- The layout algorithm should prevent this
- Track name is centered with margin
- Track number is at bottom with 3px margin
- If overlap occurs, track name may be too long

### Serial command not working:
- Check format: `/trackname <idx> <name> [actualTrack]`
- Ensure no extra spaces
- Quotes around name are optional
- Third parameter must be a number

---

## 📊 Memory Impact

**Minimal memory increase:**
- Added 1 integer array (8 integers × 4 bytes = 32 bytes)
- Added ~200 bytes of code for parsing and display
- Total: ~232 bytes

**ESP32-C3 has plenty of memory:**
- Flash: 4MB (plenty of room)
- RAM: 400KB (using ~50KB)
- No performance impact

---

## 🎉 What's Next

With this firmware update, your TDS-8 now supports:
- ✅ Track offset display
- ✅ Multiple TDS-8 units (each showing different track ranges)
- ✅ Visual confirmation of actual track numbers
- ✅ Backward compatibility with existing code

**To complete the system:**
1. ✅ Arduino firmware updated (DONE)
2. ✅ Bridge updated (DONE)
3. ✅ Web UI updated (DONE)
4. ⏳ Build M4L patch (IN PROGRESS)
5. ⏳ Test full integration

---

## 📝 Code Changes Summary

| File | Lines Changed | Description |
|------|---------------|-------------|
| `OLED30.ino` | Line 57 | Added `actualTrackNumbers` array |
| `OLED30.ino` | Lines 1404-1450 | Updated serial command parser |
| `OLED30.ino` | Lines 720-746 | Updated OSC handler |
| `OLED30.ino` | Lines 562-571 | Added track number display |
| `OLED30.ino` | Line 1520 | Updated help text |
| `OLED30.ino` | Lines 38-39 | Version bump to 0.31 |

**Total changes:** ~60 lines modified/added

---

**Updated:** October 13, 2025  
**Firmware Version:** 0.31  
**Build:** 2025.10.13-offset  
**Status:** ✅ Ready to upload and test
