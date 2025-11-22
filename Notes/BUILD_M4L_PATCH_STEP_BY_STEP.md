# Build TDS-8 M4L Patch - Step by Step

## 🎯 Goal
Create an M4L device that automatically sends track names to the TDS-8 Bridge with offset support.

---

## 📝 Quick Start (Minimal Working Version)

### Step 1: Create New M4L Device
1. In Ableton, drag **Max Audio Effect** from Max for Live onto any track
2. Click **Edit** button to open Max editor
3. You'll see an empty patch with `plugin~` object

---

## 🔧 Build the Patch (15 minutes)

### Section A: Offset Storage
1. Press `N` to create new object
2. Type `number` and press Enter
3. Label it "Offset" (double-click below to add comment)
4. This will store 0, 8, 16, 24, etc.

### Section B: Refresh Button
1. Press `N`, type `button`, press Enter
2. Resize it to be bigger (drag corner)
3. Label it "Refresh Tracks"

### Section C: Loop Through 8 Tracks
1. Below the button, create: `t b b` (press N, type "t b b")
2. Below that, create: `0` (message box with zero)
3. Below that, create: `uzi 8 0`
4. Connect: button → t b b
5. Connect: t b b (left outlet) → uzi (left inlet)
6. Connect: t b b (right outlet) → 0 → uzi (right inlet)

**What this does:** When you click the button, it loops 8 times (0-7)

### Section D: Get Loop Index
1. Below uzi, create: `unpack i i`
2. Below that, create: `t i i i` (we need the index 3 times)
3. Connect: uzi → unpack → t i i i

### Section E: Calculate Actual Track Number
1. Create a `+` object
2. Connect: offset number → + (right inlet)
3. Connect: t i i i (middle outlet) → + (left inlet)
4. This adds offset to loop index (e.g., 0+8=8, 1+8=9, etc.)

### Section F: Build Track Path
1. Create message box with: `path live_set visible_tracks $1`
2. Connect: + → this message box
3. Below it, create: `live.path`
4. Connect: message → live.path

**What this does:** Gets reference to track N in Ableton

### Section G: Get Track Name
1. Below live.path, create: `t b l`
2. Create: `live.observer`
3. Create message: `get name`
4. Connect: live.path (right outlet) → t b l
5. Connect: t b l (left) → get name → live.observer (left)
6. Connect: t b l (right) → live.observer (right)

**What this does:** Asks Live for the track's name

### Section H: Extract Name
1. Below live.observer, create: `unpack s s`
2. Connect: live.observer → unpack s s
3. The RIGHT outlet has the track name

### Section I: Package for OSC
1. Create: `pack i s i`
2. Connect: t i i i (left outlet) → pack (left inlet) [display index 0-7]
3. Connect: unpack s s (right outlet) → pack (middle inlet) [track name]
4. Connect: + → pack (right inlet) [actual track number]

**What this does:** Creates message with 3 parts: display, name, actual_track

### Section J: Send to Bridge
1. Create: `prepend /trackname`
2. Create: `udpsend 127.0.0.1 8000`
3. Create: `print OSC_OUT` (for debugging)
4. Connect: pack → prepend → udpsend
5. Connect: prepend → print (so you can see what's being sent)

**What this does:** Sends OSC message to Bridge

---

## 🎨 Visual Layout (Top to Bottom)

```
[number] ← Offset
    |
[button] ← Refresh
    |
  [t b b]
  |    \
  |    [0]
  |     |
 [uzi 8 0]
    |
[unpack i i]
    |
  [t i i i]
  |  |  |
  |  |  +--→ [+] ← (offset connects here too)
  |  |       |
  |  |       +--→ [path live_set visible_tracks $1]
  |  |       |         |
  |  |       |    [live.path]
  |  |       |         |
  |  |       |      [t b l]
  |  |       |      |    |
  |  |       |  [get name] |
  |  |       |      |      |
  |  |       |  [live.observer]
  |  |       |         |
  |  |       |    [unpack s s]
  |  |       |         |
  |  +-------+------[pack i s i]
  |                    |
  |              [prepend /trackname]
  |                    |
  +--→ [print]    [udpsend 127.0.0.1 8000]
```

---

## 🧪 Test It!

### Test 1: Basic Functionality
1. Set offset to `0`
2. Click Refresh button
3. Open Max console (Cmd+B)
4. Should see 8 messages like:
   ```
   OSC_OUT: /trackname 0 "1-Audio" 0
   OSC_OUT: /trackname 1 "2-Audio" 1
   ...
   ```

### Test 2: With Offset
1. Set offset to `8`
2. Click Refresh
3. Should see:
   ```
   OSC_OUT: /trackname 0 "9-Audio" 8
   OSC_OUT: /trackname 1 "10-Audio" 9
   ...
   ```

### Test 3: Check Bridge
1. Open Bridge console
2. Should see messages arriving:
   ```
   📨 OSC from 127.0.0.1:xxxxx → /trackname
   ✓ Forwarded to serial: /trackname 0 1-Audio (actual track: 0)
   ```

---

## 🐛 Troubleshooting

### No messages in Max console:
- Check connections (all cables should be visible)
- Make sure `print` is connected
- Click the button harder? (Sometimes Max needs a good click)

### "live.path" errors:
- Make sure message format is: `path live_set visible_tracks $1`
- Check that `$1` is there (it's a placeholder for the number)

### Wrong track names:
- Use `visible_tracks` not `tracks` (visible skips hidden tracks)
- Check that you have at least 8 tracks in Ableton

### Nothing sent to Bridge:
- Check `udpsend 127.0.0.1 8000` is correct
- Make sure Bridge is running
- Check Bridge console for errors

### Loop runs too fast:
- Add `pipe 50` between `uzi` and `unpack` to slow it down
- This gives Live API time to respond

---

## 💾 Save Your Work

1. Click the **Save** button in Max
2. Name it: `TDS-8 Auto.amxd`
3. Save in your Ableton User Library
4. Now you can drag it onto any track!

---

## 🎨 Make It Pretty (Optional)

### Add to Presentation Mode:
1. Lock patch (Cmd+E)
2. Select the number box and button
3. Right-click → "Add to Presentation"
4. Click "Presentation" button (top right)
5. Arrange the controls
6. Resize the device window

### Add Labels:
1. Create `comment` objects
2. Type descriptive text
3. Add to presentation mode

---

## 🚀 Advanced: Add Active Track Monitoring

If you want the active track to highlight:

1. Create: `live.path live_set view`
2. Create: `live.observer`
3. Create message: `get selected_track`
4. Connect them in a chain
5. This gets the currently selected track
6. Send as `/activetrack <index>` to Bridge

(This is more complex - start with the basic version first!)

---

## 📋 Quick Object Reference

| Object | What It Does |
|--------|--------------|
| `number` | Stores a number (your offset) |
| `button` | Triggers actions when clicked |
| `t b b` | Splits one bang into two |
| `uzi 8 0` | Loops 8 times, outputs 0-7 |
| `unpack i i` | Splits two numbers apart |
| `+` | Adds two numbers |
| `live.path` | Gets reference to Live objects |
| `live.observer` | Watches Live objects for changes |
| `pack i s i` | Combines int, string, int |
| `prepend` | Adds text to the front |
| `udpsend` | Sends OSC messages |
| `print` | Shows messages in console |

---

## ✅ Success Checklist

- [ ] Patch created in Max
- [ ] Offset number box works
- [ ] Refresh button works
- [ ] Messages appear in Max console
- [ ] Messages arrive at Bridge
- [ ] Track names appear on TDS-8
- [ ] Offset changes which tracks are shown
- [ ] Saved as .amxd file

---

## 🎉 You Did It!

Once this is working, you have a fully automated track display system with offset support!

**Next Steps:**
- Add keyboard shortcuts
- Add active track monitoring
- Create presets for different offsets
- Add visual feedback

---

**Pro Tip:** Start simple! Get the basic loop working first, then add the Live API stuff, then add the OSC sending. Build it piece by piece and test each section.

**Need Help?** Check the Max console (Cmd+B) - it shows all errors and messages.
