# TDS-8 Track Offset Implementation Summary

## 🎯 What Was Implemented

A complete track offset system that allows you to display different sets of 8 tracks on your TDS-8 device, enabling support for projects with more than 8 tracks and multiple TDS-8 units.

---

## 📦 Files Created

1. **`/Users/test/Music/TDS-8/tds8_automated_patch_guide.md`**
   - Comprehensive guide for building the automated M4L patch
   - Step-by-step instructions
   - Architecture diagrams
   - Troubleshooting tips

2. **`/Users/test/Music/TDS-8/tds8_simple_patch.txt`**
   - Simplified Max patch in text format
   - Can be imported directly into Max
   - Basic working example for learning

3. **`/Users/test/Music/TDS-8/TRACK_OFFSET_IMPLEMENTATION.md`**
   - This file - implementation summary

---

## 🔧 Files Modified

### 1. `/Users/test/Music/TDS-8/tds8-bridge/index.js`

#### Changes Made:

**A. Updated OSC /trackname handler (lines 73-86)**
- Now accepts 3 parameters: `displayIndex`, `name`, `actualTrack`
- Stores actual track number in memory
- Logs actual track number for debugging

```javascript
if (oscMsg.address === "/trackname" && oscMsg.args.length >= 2) {
    const displayIndex = oscMsg.args[0].value;
    const name = oscMsg.args[1].value;
    const actualTrack = oscMsg.args.length >= 3 ? oscMsg.args[2].value : displayIndex;
    
    const cmd = `/trackname ${displayIndex} ${name}\n`;
    send(cmd);
    console.log(`✓ Forwarded to serial: ${cmd.trim()} (actual track: ${actualTrack})`);
    
    // Store in memory with actual track number
    if (displayIndex >= 0 && displayIndex < 8) {
        currentTrackNames[displayIndex] = { name, actualTrack };
    }
}
```

**B. Added new API endpoint `/api/osc-send` (lines 339-361)**
- Allows web UI to send arbitrary OSC messages to M4L
- Used for the refresh button to trigger M4L
- Sends to `127.0.0.1:9000`

```javascript
app.post('/api/osc-send', (req, res) => {
  try {
    const { address, args } = req.body || {};
    if (!address) throw new Error('Missing OSC address');
    
    const port = initOSC();
    if (!port) throw new Error('OSC not initialized');
    
    const message = new osc.OSCMessage(address);
    if (args && Array.isArray(args)) {
      args.forEach(arg => message.add(arg));
    }
    
    port.send(message, "127.0.0.1", 9000);
    console.log(`📡 OSC sent to M4L: ${address}`, args || []);
    
    res.json({ ok: true, message: `Sent ${address} to M4L` });
  } catch (e) { 
    console.error('OSC send error:', e.message);
    res.status(400).json({ error: e.message }); 
  }
});
```

---

### 2. `/Users/test/Music/TDS-8/tds8-bridge/web/index.html`

#### Changes Made:

**A. Added Offset Controls UI (lines 94-103)**
- Left/Right arrow buttons to change offset
- Display showing current offset and track range
- Visual feedback for which tracks are being shown

```html
<!-- Offset Controls -->
<div style="display:flex;align-items:center;gap:12px;margin-bottom:16px;padding:12px;background:#fafbff;border-radius:8px;border:2px solid #e3e6ef">
  <button id="btnOffsetLeft" class="btn ghost sm" title="Previous 8 tracks">◀</button>
  <div style="flex:1;text-align:center">
    <div style="font-size:11px;color:var(--muted);margin-bottom:4px">Track Offset</div>
    <div style="font-size:18px;font-weight:600;color:var(--brand)" id="offsetDisplay">0</div>
    <div style="font-size:10px;color:var(--muted);margin-top:2px">Showing tracks <span id="offsetRange">1-8</span></div>
  </div>
  <button id="btnOffsetRight" class="btn ghost sm" title="Next 8 tracks">▶</button>
</div>
```

**B. Added Refresh Button (line 108)**
```html
<button id="btnRefresh" class="btn primary">🔄 Refresh from Ableton</button>
```

**C. Added JavaScript for Offset Management (lines 352-368)**
- `currentOffset` variable to track current offset
- `updateOffsetDisplay()` function to update UI
- Updates track labels to show actual track numbers

```javascript
let currentOffset = 0;

function updateOffsetDisplay() {
  $('#offsetDisplay').textContent = currentOffset;
  const start = currentOffset + 1;
  const end = currentOffset + 8;
  $('#offsetRange').textContent = `${start}-${end}`;
  
  // Update track labels to show actual track numbers
  for (let i = 0; i < 8; i++) {
    const label = document.querySelector(`#tracks .trackrow:nth-child(${i + 1}) label`);
    if (label) {
      const actualTrack = currentOffset + i + 1;
      label.textContent = `Track ${actualTrack}`;
    }
  }
}
```

**D. Modified buildTrackGrid() (lines 370-392)**
- Now uses `currentOffset` to calculate actual track numbers
- Updates labels dynamically

**E. Added Button Handlers (lines 422-460)**
- Left arrow: Decreases offset by 8 (minimum 0)
- Right arrow: Increases offset by 8
- Refresh: Sends `/refresh` OSC message to M4L with current offset

```javascript
// Offset controls
$('#btnOffsetLeft').onclick = () => {
  if (currentOffset > 0) {
    currentOffset -= 8;
    updateOffsetDisplay();
    log(`◀ Offset: ${currentOffset} (tracks ${currentOffset + 1}-${currentOffset + 8})`, 'info');
  }
};

$('#btnOffsetRight').onclick = () => {
  currentOffset += 8;
  updateOffsetDisplay();
  log(`▶ Offset: ${currentOffset} (tracks ${currentOffset + 1}-${currentOffset + 8})`, 'info');
};

// Refresh button
$('#btnRefresh').onclick = async () => {
  log(`🔄 Requesting track refresh from Ableton (offset: ${currentOffset})...`, 'info');
  
  try {
    const res = await fetch('/api/osc-send', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({
        address: '/refresh',
        args: [currentOffset]
      })
    });
    
    if (res.ok) {
      log(`✓ Refresh request sent to Ableton`, 'out');
    } else {
      log(`✗ Failed to send refresh request`, 'err');
    }
  } catch (err) {
    log(`✗ Error: ${err.message}`, 'err');
  }
};
```

---

## 🎹 Max for Live (M4L) Implementation

### What You Need to Build:

Your M4L device needs to:

1. **Listen for `/refresh` OSC message** with offset parameter
2. **Loop through 8 tracks** starting at offset
3. **Get track names** from Live API
4. **Send `/trackname` messages** with 3 parameters:
   - Display index (0-7)
   - Track name
   - Actual track number

### Message Format:
```
/trackname <display_index> <track_name> <actual_track_number>
```

### Example Flow:

**Offset = 0 (tracks 1-8):**
```
/trackname 0 "Kick" 0
/trackname 1 "Snare" 1
/trackname 2 "Hi-Hat" 2
...
/trackname 7 "Synth" 7
```

**Offset = 8 (tracks 9-16):**
```
/trackname 0 "Pad" 8
/trackname 1 "Bass" 9
/trackname 2 "Lead" 10
...
/trackname 7 "FX" 15
```

### Key Max Objects Needed:

```
[udpreceive 9000]  ← Listen for /refresh from Bridge
|
[route /refresh]
|
[unpack i]  ← Get offset value
|
[uzi 8]  ← Loop 8 times
|
[+ $offset]  ← Add offset to get actual track number
|
[pack path live_set visible_tracks 0]
|
[live.path] → [live.observer] → get track name
|
[pack i s i]  ← Pack: display_index, name, actual_track
|
[prepend /trackname]
|
[udpsend 127.0.0.1 8000]  ← Send to Bridge
```

---

## 🎨 Arduino Firmware (Future Enhancement)

To display actual track numbers on the OLEDs, you'll need to modify the Arduino code to:

1. **Accept 3rd parameter** in `/trackname` command (actual track number)
2. **Display track number** in small text at bottom of OLED
3. **Display track name** in center as usual

### Example OLED Layout:
```
┌──────────────┐
│              │
│    Kick      │  ← Track name (center, large)
│              │
│      1       │  ← Track number (bottom, small)
└──────────────┘
```

### Arduino Code Modification Needed:

In `OLED30.ino`, update the `/trackname` command handler:

```cpp
// Current format: /trackname <index> <name>
// New format: /trackname <index> <name> <actual_track>

if (msg.fullMatch("/trackname")) {
  int idx = msg.getInt(0);
  String name = msg.getString(1);
  int actualTrack = msg.getInt(2);  // NEW: Get actual track number
  
  if (idx >= 0 && idx < 8) {
    trackNames[idx] = name;
    actualTrackNumbers[idx] = actualTrack;  // NEW: Store actual track number
    drawDisplay(idx);
  }
}
```

Then in `drawDisplay()`:

```cpp
void drawDisplay(int idx) {
  displays[idx]->clearDisplay();
  
  // Draw track name (center, large font)
  displays[idx]->setTextSize(2);
  displays[idx]->setCursor(centerX, centerY);
  displays[idx]->print(trackNames[idx]);
  
  // Draw actual track number (bottom, small font)
  displays[idx]->setTextSize(1);
  displays[idx]->setCursor(bottomCenterX, 56);  // Near bottom
  displays[idx]->print(actualTrackNumbers[idx] + 1);  // +1 for 1-based display
  
  displays[idx]->display();
}
```

---

## 🧪 Testing the Implementation

### Test 1: Bridge UI Works
1. Start the bridge: `cd tds8-bridge && node index.js`
2. Open http://localhost:8088
3. Click left/right arrows - offset should change
4. Track labels should update (Track 1-8, Track 9-16, etc.)

### Test 2: Refresh Button
1. Click "🔄 Refresh from Ableton"
2. Check bridge console - should see: `📡 OSC sent to M4L: /refresh [0]`
3. (M4L device needs to be built to respond)

### Test 3: OSC API Endpoint
```bash
curl -X POST http://localhost:8088/api/osc-send \
  -H "Content-Type: application/json" \
  -d '{"address":"/refresh","args":[8]}'
```

Should see in bridge console:
```
📡 OSC sent to M4L: /refresh [ 8 ]
```

### Test 4: Full Flow (Once M4L is Built)
1. Set offset to 0 in Bridge UI
2. Click Refresh
3. M4L should send tracks 0-7 to Bridge
4. Bridge forwards to Arduino
5. OLEDs display tracks 1-8

6. Set offset to 8 in Bridge UI
7. Click Refresh
8. M4L should send tracks 8-15 to Bridge
9. Bridge forwards to Arduino
10. OLEDs display tracks 9-16

---

## 📋 Next Steps

### Immediate:
1. ✅ Bridge updated (DONE)
2. ✅ Web UI updated (DONE)
3. ⏳ Build M4L patch using the guide
4. ⏳ Test M4L → Bridge communication
5. ⏳ Update Arduino firmware to show track numbers

### Future Enhancements:
- Visual indicator in Ableton showing which 8 tracks are displayed
- Keyboard shortcuts for offset (Cmd+Left/Right)
- Auto-refresh when tracks are added/removed
- Save offset preference per project
- Support for multiple TDS-8 units with different offsets

---

## 🐛 Troubleshooting

### Bridge doesn't receive /refresh
- Check M4L is sending to `127.0.0.1:9000`
- Check Bridge console for OSC messages
- Verify OSC listener is running on port 9000

### Track names not updating
- Check M4L is sending to `127.0.0.1:8000`
- Verify Bridge is forwarding to serial
- Check Arduino is receiving commands

### Offset buttons don't work
- Check browser console for errors
- Verify JavaScript is loaded
- Test with browser dev tools

### Wrong track numbers displayed
- Verify M4L is sending 3 parameters
- Check Bridge logs for actual track numbers
- Ensure Arduino firmware is updated

---

## 📞 Support

If you encounter issues:
1. Check Bridge console logs
2. Check browser console (F12)
3. Check M4L Max window console
4. Check Arduino serial monitor

All components log their activity for debugging.

---

## 🎉 Success Criteria

You'll know it's working when:
1. ✅ Offset controls change the track range display
2. ✅ Refresh button triggers M4L
3. ✅ M4L sends track names with correct parameters
4. ✅ Bridge forwards to Arduino
5. ✅ OLEDs show correct track names
6. ✅ OLEDs show actual track numbers at bottom
7. ✅ Can switch between different sets of 8 tracks seamlessly

---

**Implementation Date:** October 13, 2025
**Status:** Bridge & Web UI Complete, M4L & Arduino Pending
