# TDS-8 Automated M4L Patch Guide

## Overview
This guide shows how to build an automated M4L device that:
1. Automatically detects and sends track names (no manual wiring)
2. Supports track offset (show tracks 0-7, 8-15, 16-23, etc.)
3. Sends actual track numbers to Arduino for display
4. Has a refresh button to update all displays

## Key Concepts

### Message Format to Bridge:
```
/trackname <display_index> <track_name> <actual_track_number>
```

Example:
- `/trackname 0 "Kick" 0` → Display slot 0 shows "Kick" (track 0)
- `/trackname 0 "Snare" 8` → Display slot 0 shows "Snare" (track 8) when offset=8

---

## Patch Architecture

### Section 1: Offset Control
```
┌─────────────────────────────────────┐
│  [number]  ← Offset (0, 8, 16, 24)  │
│     |                                │
│  [* 1]  ← Store and pass through    │
│     |                                │
│  [t i i]  ← Trigger to loop & active│
└─────────────────────────────────────┘
```

### Section 2: Refresh & Loop System
```
┌──────────────────────────────────────────┐
│  [button "Refresh All Tracks"]           │
│     |                                     │
│  [t b b]                                  │
│   |   \                                   │
│   |    [0]  ← Reset counter               │
│   |     |                                 │
│  [uzi 8 0]  ← Loop 8 times (0-7)         │
│     |                                     │
│  [unpack i i]  ← Get loop index          │
│     |                                     │
│  [t i i i]  ← Need index 3 times         │
│   | | |                                   │
│   | | +--→ [+ $offset] → actual_track    │
│   | |                                     │
│   | +----→ display_index (0-7)           │
│   |                                       │
│   +------→ [pack path live_set ...]      │
└──────────────────────────────────────────┘
```

### Section 3: Get Track Name
```
┌──────────────────────────────────────────────────────┐
│  [pack path live_set visible_tracks 0 view ...]      │
│     |                                                 │
│  [live.path]                                          │
│     |                                                 │
│  [t b l]                                              │
│   |   |                                               │
│   |   +→ [live.observer @_persistence 1]             │
│   |          |                                        │
│   +→ [get name]                                       │
│          |                                            │
│  [unpack s s]  ← Extract "name" "TrackName"          │
│          |                                            │
│       (second outlet is the track name)               │
└──────────────────────────────────────────────────────┘
```

### Section 4: Format & Send OSC
```
┌────────────────────────────────────────┐
│  [pack i s i]  ← display, name, actual │
│     |                                   │
│  [prepend /trackname]                  │
│     |                                   │
│  [udpsend 127.0.0.1 8000]              │
└────────────────────────────────────────┘
```

### Section 5: Active Track Monitoring
```
┌────────────────────────────────────────┐
│  [live.path live_set view]             │
│     |                                   │
│  [live.observer]                        │
│     |                                   │
│  [get selected_track]                  │
│     |                                   │
│  [live.path]                            │
│     |                                   │
│  [live.object]                          │
│     |                                   │
│  [get id]                               │
│     |                                   │
│  [M4L.api.RemoteMatchIdToIndex]        │
│     |                                   │
│  [- $offset]  ← Subtract offset        │
│     |                                   │
│  [prepend /activetrack]                │
│     |                                   │
│  [udpsend 127.0.0.1 8000]              │
└────────────────────────────────────────┘
```

---

## Step-by-Step Build Instructions

### 1. Create New M4L Audio Effect
1. In Ableton, create a new MIDI or Audio track
2. Drag "Max Audio Effect" from Max for Live
3. Click "Edit" to open Max editor

### 2. Add Offset Control (Top Section)
1. Press `N` to create new object
2. Type `number` and press Enter
3. Create `t i i` below it
4. Connect number → t i i
5. Label the number box "Track Offset"

### 3. Add Refresh Button
1. Create `button` object
2. Create `t b b` below it
3. Create `0` (message box with "0")
4. Create `uzi 8 0`
5. Connect: button → t b b
6. Connect: t b b (left) → uzi
7. Connect: t b b (right) → 0 → uzi (right inlet)

### 4. Add Loop Processing
1. Create `unpack i i` below uzi
2. Create `t i i i` below unpack
3. Connect uzi → unpack → t i i i

### 5. Add Track Path Builder
1. Create `+` object (this adds offset to loop index)
2. Connect: offset number (from step 2) → + (right inlet)
3. Connect: t i i i (right outlet) → + (left inlet)
4. Create message: `pack path live_set visible_tracks 0`
5. Connect: + → pack (replace the 0)

### 6. Add Track Name Getter
1. Create `live.path`
2. Create `t b l`
3. Create `live.observer @_persistence 1`
4. Create message `get name`
5. Create `unpack s s`
6. Connect chain: pack → live.path → t b l
7. Connect: t b l (left) → get name → live.observer
8. Connect: t b l (right) → live.observer (right inlet)
9. Connect: live.observer → unpack s s

### 7. Add OSC Formatter
1. Create `pack i s i` (for display_index, name, actual_track)
2. Connect: t i i i (left) → pack i s i (left inlet) [display index 0-7]
3. Connect: unpack s s (right) → pack i s i (middle inlet) [track name]
4. Connect: + output → pack i s i (right inlet) [actual track number]
5. Create `prepend /trackname`
6. Create `udpsend 127.0.0.1 8000`
7. Connect: pack → prepend → udpsend

### 8. Add Active Track Monitor (Optional)
This sends which track is selected in Ableton:
1. Create `live.path live_set view`
2. Create `live.thisdevice` → connect to live.path
3. Create `live.observer` chain (see Section 5 above)
4. Use `M4L.api.GetSelectedTrackIndex` abstraction (you already have this!)
5. Subtract offset to get display index (0-7)
6. Send as `/activetrack <index>`

---

## Testing

### Test 1: Basic Refresh
1. Click the Refresh button
2. Check Bridge console - should see 8 `/trackname` messages
3. Example output:
   ```
   /trackname 0 "1-Audio" 0
   /trackname 1 "2-Audio" 1
   ...
   /trackname 7 "8-Audio" 7
   ```

### Test 2: Offset
1. Change offset number to 8
2. Click Refresh
3. Should now see tracks 8-15:
   ```
   /trackname 0 "9-Audio" 8
   /trackname 1 "10-Audio" 9
   ...
   ```

### Test 3: Active Track
1. Click different tracks in Ableton
2. Should see `/activetrack <0-7>` messages
3. Only highlights if track is in current offset range

---

## Simplified Starter Patch

If the above is too complex, here's a minimal version:

```
[loadbang]
|
[t b b]
|    \
|     [0]
|      |
[uzi 8]
|
[unpack i i]
|
[pack path live_set visible_tracks 0]
|
[live.path]
|
[t b l]
|    |
|    +→ [live.observer]
|           |
+→ [get name]
       |
   [unpack s s]
       |
   [prepend /trackname 0]  ← Hardcode display index for now
       |
   [udpsend 127.0.0.1 8000]
```

This will send all track names on device load, all to display 0 (for testing).

---

## Next Steps

1. Build the basic loop system first (Steps 1-4)
2. Test that it loops 8 times
3. Add track name fetching (Steps 5-6)
4. Add OSC sending (Step 7)
5. Add offset control
6. Add active track monitoring

## Troubleshooting

**Problem:** No messages sent
- Check udpsend port (should be 8000)
- Check Bridge is running
- Add `print` objects to debug

**Problem:** Wrong track names
- Check `visible_tracks` vs `tracks` in live.path
- Use `visible_tracks` to skip hidden tracks

**Problem:** Offset not working
- Make sure `+` object gets offset value
- Check connections to pack i s i

**Problem:** Loop runs too fast
- Add `pipe 50` between uzi and unpack to delay each iteration
- This gives Live API time to respond

---

## Advanced: Add to Presentation Mode

1. Lock patch (Cmd+E)
2. Select objects to show in device
3. Right-click → "Add to Presentation"
4. Enter Presentation mode (View → Presentation)
5. Arrange UI elements
6. Resize device window

Recommended presentation elements:
- Offset number box
- Refresh button
- Status message box (shows last update)
