# TDS-8 v0.32 - Startup Fix

## 🐛 Issues Fixed

### 1. **Removed "USB READY" Blocking Screen**
**Problem:** On startup, display 7 showed "USB READY" and blocked all updates  
**Fix:** Removed `showWiredReady()` call, replaced with immediate display of all tracks

**Changed in setup():**
```cpp
// OLD:
showWiredReady();     // Show "USB READY" on displays
refreshAll();         // Show track names

// NEW:
// Draw all displays with just borders and track numbers
for (uint8_t i = 0; i < numScreens; i++) {
  drawTrackName(i, trackNames[i]);
}
```

**Result:**
- ✅ No blocking startup screen
- ✅ All 8 displays immediately show borders + track numbers (1-8)
- ✅ Ready to receive track names instantly

---

### 2. **Border Positioning Confirmed**
**Question:** Is the border at the outer edge or inset?  
**Answer:** Border is at the **outer edge** (correct)

```cpp
display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
```

- `(0, 0)` = top-left corner (outer edge)
- `SCREEN_WIDTH - 1` = 127 (right edge)
- `SCREEN_HEIGHT - 1` = 63 (bottom edge)

**No buffer/inset** - border uses the full display area.

---

## 📺 Display Behavior

### **On Startup (Immediately):**
```
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│              │  │              │  │              │
│              │  │              │  │              │
│              │  │              │  │              │
│      1       │  │      2       │  │      3       │
└──────────────┘  └──────────────┘  └──────────────┘
   Display 0         Display 1         Display 2
```

All 8 displays show:
- Border at outer edge
- Track number at bottom (1, 2, 3, 4, 5, 6, 7, 8)
- Empty center (ready for track names)

### **After Receiving Track Name:**
```
┌──────────────┐
│              │
│    Kick      │  ← Track name (centered)
│              │
│      9       │  ← Actual track number
└──────────────┘
```

---

## 🚀 Upload Instructions

1. **Open Arduino IDE**
2. **Open:** `/Users/test/Music/TDS-8/OLED32/OLED32.ino`
3. **Verify port:** Tools → Port → `/dev/cu.usbmodem14301`
4. **Upload**
5. **Test:**
   - All displays should immediately show borders + numbers
   - No "USB READY" screen
   - No blocking/waiting

---

## 🧪 Test Commands

### Test 1: Verify Version
```bash
# In Serial Monitor (115200 baud):
VERSION

# Expected:
VERSION: 0.32
BUILD: 2025.10.14-offset
```

### Test 2: Send Track Name
```bash
/trackname 0 "Kick"

# Expected:
# - Display 0 shows "Kick" in center
# - Display 0 shows "1" at bottom
```

### Test 3: With Offset
```bash
/trackname 0 "Snare" 8

# Expected:
# - Display 0 shows "Snare" in center
# - Display 0 shows "9" at bottom
```

---

## ✅ What's Fixed

- ✅ No startup blocking screen
- ✅ Displays immediately ready
- ✅ All 8 displays show borders + numbers on boot
- ✅ Track names update instantly when received
- ✅ Border at outer edge (no inset)
- ✅ Version updated to 0.32

---

## 📝 Files Changed

**File:** `/Users/test/Music/TDS-8/OLED32/OLED32.ino`

**Changes:**
- Line 203: Updated startup message to v0.32
- Lines 227-242: Removed `showWiredReady()`, replaced with immediate `drawTrackName()` loop

**Lines of code changed:** ~5 lines

---

**Date:** October 14, 2025  
**Version:** 0.32  
**Status:** ✅ Ready to upload and test
