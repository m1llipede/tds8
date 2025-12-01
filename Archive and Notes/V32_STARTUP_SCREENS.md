# TDS-8 v0.32 - Startup Screen Sequence

## 📺 Complete Startup Sequence

---

### **Screen 1: TDS-8 Logo (3 seconds)**

**Duration:** 3 seconds  
**Borders:** None  
**Layout:**

```
[    ] [ T  ] [ D  ] [ S  ] [ -  ] [ 8  ] [    ] [Logo]
  0      1      2      3      4      5      6      7
```

**Details:**
- Display 0: Empty
- Displays 1-5: T D S - 8 (text size 6 - HUGE!)
- Display 6: Empty
- Display 7: PlayOptix logo
- No borders on any display
- Clean, bold branding

---

### **Screen 2: Instructions (Stays until track names arrive)**

**Duration:** Indefinite (until first track name received)  
**Borders:** None  
**Layout:**

```
[Launch] [the   ] [App   ] [Connect] [to the] [TDS-8 ] [Launch ] [Logo]
         [TDS-8 ]                                       [Ableton]
   0        1        2         3         4        5        6        7
```

**Text on each display:**
- **Display 0:** "Launch"
- **Display 1:** "the" / "TDS-8" (two lines)
- **Display 2:** "App"
- **Display 3:** "Connect"
- **Display 4:** "to the"
- **Display 5:** "TDS-8"
- **Display 6:** "Launch" / "Ableton" (two lines)
- **Display 7:** PlayOptix logo (persistent)

**Reading across:**
> "Launch the TDS-8 App, Connect to the TDS-8, Launch Ableton"

---

### **Screen 3: Track Display Mode (After first track name)**

**Triggered by:** First `/trackname` command received  
**Borders:** Yes (on all displays)  
**Layout:**

```
┌──────────┐ ┌──────────┐ ┌──────────┐
│          │ │          │ │          │
│  Kick    │ │  Snare   │ │  Bass    │
│          │ │          │ │          │
│ Track 1  │ │ Track 2  │ │ Track 3  │
└──────────┘ └──────────┘ └──────────┘
```

**Features:**
- Borders on all displays
- Track names centered (large text)
- Track numbers at bottom ("Track 1", "Track 2", etc.)
- Supports offset (can show "Track 9", "Track 17", etc.)

---

## 🎨 Visual Timeline

```
Power On
   ↓
┌─────────────────────────────────────┐
│  Screen 1: TDS-8 Logo (3 seconds)  │
│  [ ] [T] [D] [S] [-] [8] [ ] [Logo]│
│  No borders, huge letters           │
└─────────────────────────────────────┘
   ↓ (3 seconds)
┌─────────────────────────────────────┐
│  Screen 2: Instructions (hold)     │
│  Launch the TDS-8 App...            │
│  Connect to the TDS-8...            │
│  Launch Ableton...                  │
│  No borders, readable text          │
└─────────────────────────────────────┘
   ↓ (when track names received)
┌─────────────────────────────────────┐
│  Screen 3: Track Display (active)  │
│  ┌──────┐ ┌──────┐ ┌──────┐        │
│  │ Kick │ │Snare │ │ Bass │        │
│  │Track1│ │Track2│ │Track3│        │
│  └──────┘ └──────┘ └──────┘        │
└─────────────────────────────────────┘
```

---

## 🔧 Technical Details

### **Text Sizes:**
- **TDS-8 Logo:** Size 6 (36x48 pixels per character)
- **Instructions:** Size 2 (12x16 pixels per character)
- **Track Names:** Size 2 (auto-shrinks to size 1 if needed)
- **Track Numbers:** Size 1 (6x8 pixels)

### **Borders:**
- **Screen 1 (Logo):** No borders
- **Screen 2 (Instructions):** No borders
- **Screen 3 (Tracks):** Borders on all displays
- **Border position:** Outer edge (0,0 to 127,63)

### **Timing:**
- **Logo screen:** 3 seconds (hardcoded delay)
- **Instruction screen:** Indefinite (until first track name)
- **Track screen:** Persistent (updates as needed)

---

## 📝 Code Changes

**File:** `/Users/test/Music/TDS-8/OLED32/OLED32.ino`

**Function:** `showStartupSplash()` (lines 1326-1444)

**Changes:**
1. Reduced logo display time from 5s to 3s
2. Removed all borders from splash screens
3. Added instruction screen with 7 displays of text
4. PlayOptix logo persists across both screens
5. Instructions stay until first track name arrives

**Total lines:** ~120 lines (was ~40)

---

## 🎯 User Experience Flow

### **First Boot:**
1. User powers on TDS-8
2. Sees bold "TDS-8" branding for 3 seconds
3. Sees instructions: "Launch the TDS-8 App, Connect to the TDS-8, Launch Ableton"
4. User follows instructions
5. When Ableton sends track names → switches to track display

### **Subsequent Boots:**
1. User powers on TDS-8
2. Sees "TDS-8" branding for 3 seconds
3. Sees instructions briefly
4. If app/Ableton already running → track names appear quickly
5. Seamless transition to working state

---

## 🧪 Testing Checklist

- [ ] Upload OLED32.ino
- [ ] Power on device
- [ ] **Screen 1 (3 seconds):**
  - [ ] Displays 1-5 show T D S - 8
  - [ ] Display 7 shows PlayOptix logo
  - [ ] No borders visible
  - [ ] Letters are huge (size 6)
- [ ] **Screen 2 (indefinite):**
  - [ ] Display 0: "Launch"
  - [ ] Display 1: "the" / "TDS-8"
  - [ ] Display 2: "App"
  - [ ] Display 3: "Connect"
  - [ ] Display 4: "to the"
  - [ ] Display 5: "TDS-8"
  - [ ] Display 6: "Launch" / "Ableton"
  - [ ] Display 7: PlayOptix logo (still there)
  - [ ] No borders visible
- [ ] **Screen 3 (after track names):**
  - [ ] Send `/trackname 0 "Test"`
  - [ ] All displays switch to track mode
  - [ ] Borders appear
  - [ ] Track numbers show at bottom

---

## 💡 Design Rationale

### **Why 3 seconds for logo?**
- Long enough to see branding
- Short enough to not feel slow
- Professional product feel

### **Why no borders on startup?**
- Cleaner, more modern look
- Focuses attention on content
- Borders reserved for "working mode"

### **Why persistent instructions?**
- New users need guidance
- No time pressure
- Clear call-to-action
- Automatically disappears when system is working

### **Why keep PlayOptix logo throughout?**
- Consistent branding
- Professional appearance
- Utilizes otherwise empty display

---

**Date:** October 14, 2025  
**Version:** 0.32  
**Status:** ✅ Complete and ready to test!
