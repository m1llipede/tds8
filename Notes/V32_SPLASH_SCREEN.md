# TDS-8 v0.32 - Custom Splash Screen

## ✨ New Startup Splash Screen

### **What Shows on Boot:**

```
Display 0: [Empty border]
Display 1: [Empty border]
Display 2: Large "T"
Display 3: Large "D"
Display 4: Large "S"
Display 5: Large "-"
Display 6: Large "8"
Display 7: "Play" / "Optix"
```

### **Visual Layout:**

```
┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐
│    │ │    │ │    │ │    │ │    │ │    │ │    │ │Play│
│    │ │    │ │ T  │ │ D  │ │ S  │ │ -  │ │ 8  │ │────│
│    │ │    │ │    │ │    │ │    │ │    │ │    │ │Optix│
└────┘ └────┘ └────┘ └────┘ └────┘ └────┘ └────┘ └────┘
  0      1      2      3      4      5      6      7
```

**Duration:** 2 seconds, then switches to track display mode

---

## 🔧 Changes Made

### 1. **Replaced `showWiredReady()` with `showStartupSplash()`**

**Old function:**
- Showed "USB READY" on display 7
- Blocked updates

**New function:**
- Shows "T D S - 8" across displays 2-6 (text size 4 - huge!)
- Shows "PlayOptix" on display 7 (text size 2)
- Displays 0-1 remain empty with borders
- Auto-clears after 2 seconds

### 2. **Updated Track Number Display**

**Changed:**
```cpp
// OLD:
String trackNumStr = String(actualTrackNumbers[screen] + 1); // "1", "2", "3"

// NEW:
String trackNumStr = "Track " + String(actualTrackNumbers[screen] + 1); // "Track 1", "Track 2"
```

### 3. **Removed WiFi Setup Screens**

The `showNetworkSetup()` function still exists for WiFi mode, but in wired mode (default):
- ✅ No "INITIAL SETUP" screens
- ✅ No WiFi configuration prompts
- ✅ Clean splash → track display

---

## 📺 Complete Boot Sequence

### **Step 1: Power On (0.5s)**
All displays show empty borders

### **Step 2: Splash Screen (2s)**
```
[  ] [  ] [ T ] [ D ] [ S ] [ - ] [ 8 ] [Play/Optix]
```

### **Step 3: Track Display Mode (Ready)**
```
┌──────────┐ ┌──────────┐ ┌──────────┐
│          │ │          │ │          │
│          │ │          │ │          │
│          │ │          │ │          │
│ Track 1  │ │ Track 2  │ │ Track 3  │
└──────────┘ └──────────┘ └──────────┘
```

All displays ready to receive track names!

---

## 🎨 Display Details

### **TDS-8 Letters (Displays 2-6):**
- **Text Size:** 4 (maximum size)
- **Position:** Centered vertically and horizontally
- **Border:** Full screen border
- **Characters:** T, D, S, -, 8 (one per display)

### **PlayOptix (Display 7):**
- **Text Size:** 2
- **Lines:** 
  - "Play" at y=18
  - "Optix" at y=38
- **Position:** Centered horizontally
- **Border:** Full screen border

### **Empty Displays (0-1):**
- Just borders, no text
- Ready for future use or track names

---

## 🚀 Upload and Test

1. **Upload** `OLED32.ino`
2. **Watch the boot sequence:**
   - Borders appear
   - Splash shows for 2 seconds
   - Switches to track mode
3. **Verify:**
   - No WiFi setup screens
   - Clean TDS-8 branding
   - "Track 1", "Track 2", etc. at bottom

---

## 📝 Code Summary

**New Function:** `showStartupSplash()` (lines 1321-1365)
- 45 lines of code
- Displays TDS-8 branding
- 2-second delay
- Auto-clears to track mode

**Modified:**
- Line 167: Function declaration
- Line 228: Call in setup()
- Line 570: Track number display format

**Total changes:** ~50 lines

---

**Date:** October 14, 2025  
**Version:** 0.32  
**Status:** ✅ Ready to upload
