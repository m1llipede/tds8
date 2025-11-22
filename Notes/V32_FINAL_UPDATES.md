# TDS-8 v0.32 - Final Updates

## ✅ Changes Made (October 14, 2025)

---

### 1. **Splash Screen Starts at Display 1 (Not 2)**

**Changed:**
```cpp
// OLD:
tcaSelect(i + 2); // Start at display 2

// NEW:
tcaSelect(i + 1); // Start at display 1
```

**Result:**
```
Display 0: [Empty]
Display 1: T
Display 2: D
Display 3: S
Display 4: -
Display 5: 8
Display 6: [Empty]
Display 7: PlayOptix logo
```

---

### 2. **Removed Borders from Splash Screen**

**Changed:**
- Removed `display.drawRect()` calls from splash screen
- Splash now shows clean letters/logo without borders
- Main track display still has borders

**Splash (no borders):**
```
┌────────┐
│        │
│   T    │  ← No border
│        │
└────────┘
```

**Track Display (with borders):**
```
┌────────┐
│        │  ← Border at edges
│  Kick  │
│Track 1 │
└────────┘
```

---

### 3. **Removed WiFi Setup Text**

**Old behavior:**
- Showed "INITIAL SETUP", "Connect to TDS8 WiFi", etc.
- 8 screens of instructions

**New behavior:**
- Just blank displays with borders
- No confusing WiFi text
- Clean and simple

**Function simplified:**
```cpp
void showNetworkSetup() {
  // WiFi setup screens removed - just show blank displays with borders
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    display.display();
  }
}
```

---

### 4. **Border Position Confirmed**

**Question:** Are borders at the very edges (starting at 0,0)?  
**Answer:** ✅ **YES!**

**Code:**
```cpp
display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
```

**Breakdown:**
- `0, 0` = Top-left corner (pixel 0,0)
- `SCREEN_WIDTH - 1` = 127 (rightmost pixel)
- `SCREEN_HEIGHT - 1` = 63 (bottom pixel)
- Display is 128x64, so pixels are 0-127 (width) and 0-63 (height)

**Visual:**
```
(0,0)────────────────(127,0)
  │                      │
  │    128x64 OLED      │
  │                      │
(0,63)───────────────(127,63)
```

**Border is drawn:**
- Top edge: y=0
- Left edge: x=0
- Right edge: x=127
- Bottom edge: y=63

✅ **Borders are at the absolute outer edges of the display!**

---

## 📺 Complete Boot Sequence

### **Step 1: Power On**
All displays show empty borders

### **Step 2: Splash Screen (5 seconds)**
```
[  ] [ T ] [ D ] [ S ] [ - ] [ 8 ] [  ] [Logo]
 0    1    2    3    4    5    6    7
```
- No borders on splash
- Clean, bold letters
- PlayOptix logo on display 7

### **Step 3: Track Display Mode**
```
┌──────────┐ ┌──────────┐ ┌──────────┐
│          │ │          │ │          │
│          │ │          │ │          │
│          │ │          │ │          │
│ Track 1  │ │ Track 2  │ │ Track 3  │
└──────────┘ └──────────┘ └──────────┘
```
- Borders on all displays
- Track numbers at bottom
- Ready for track names

---

## 🎯 Summary of All Changes

| Feature | Status | Details |
|---------|--------|---------|
| Splash starts at display 1 | ✅ | Was display 2, now display 1 |
| No borders on splash | ✅ | Clean look, borders only on track mode |
| WiFi setup text removed | ✅ | No more confusing instructions |
| Borders at edges confirmed | ✅ | Starts at (0,0), goes to (127,63) |
| Track numbers show "Track N" | ✅ | Not just "1", shows "Track 1" |
| 5-second splash duration | ✅ | Plenty of time to see branding |
| Logo support added | ✅ | Ready for PlayOptix bitmap |

---

## 📝 Files Modified

**File:** `/Users/test/Music/TDS-8/OLED32/OLED32.ino`

**Changes:**
1. Line 1343: Changed `i + 2` to `i + 1` (start at display 1)
2. Lines 1348, 1365: Removed `drawRect()` from splash
3. Lines 628-637: Simplified `showNetworkSetup()` (no text)
4. Line 570: Track numbers show "Track N" format

**Total lines changed:** ~15 lines

---

## 🧪 Testing Checklist

- [ ] Upload OLED32.ino
- [ ] Power on device
- [ ] Verify splash starts at display 1 (not 2)
- [ ] Verify no borders on splash screen
- [ ] Verify 5-second splash duration
- [ ] Verify track mode has borders
- [ ] Verify "Track 1", "Track 2", etc. at bottom
- [ ] No WiFi setup text appears

---

## 📐 Display Specifications

**OLED Display:**
- Resolution: 128x64 pixels
- Pixel coordinates: (0,0) to (127,63)
- Border drawn at outer edges
- Text size 1: 6x8 pixels per character
- Text size 2: 12x16 pixels per character
- Text size 4: 24x32 pixels per character

**Border Details:**
- Line thickness: 1 pixel
- Color: White (SSD1306_WHITE)
- Position: Outer edge of display
- No inset or padding

---

**Date:** October 14, 2025  
**Version:** 0.32  
**Status:** ✅ All updates complete and ready to test!
