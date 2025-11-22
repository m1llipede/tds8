# TDS-8 Project Analysis & Version History

## Project Overview
**TDS-8** is a Track Display Strip system for Ableton Live integration using:
- **Hardware**: Seeed XIAO ESP32-C3 microcontroller
- **Displays**: 8x SSD1306 OLED screens (128x64) via TCA9548A I²C multiplexer
- **Communication**: OSC over UDP (WiFi) or Serial over USB-C (Wired)
- **Integration**: Max for Live device (`tds8.amxd`) in Ableton Live
- **Desktop Bridge**: Node.js application (`tds8-bridge`) for USB serial relay

## Key Components

### Hardware Stack
1. **ESP32-C3 XIAO** - Main controller with USB-C
2. **8x SSD1306 OLED** - I²C displays (0x3C address)
3. **TCA9548A** - I²C multiplexer (0x70 address) for 8 displays
4. **I²C Bus** - 400kHz fast mode

### Software Architecture
1. **Firmware** (Arduino/ESP32) - Runs on XIAO
2. **Desktop Bridge** (Node.js) - Serial-to-Web relay on computer
3. **Max for Live Device** - Ableton integration
4. **Web Dashboard** - Configuration UI (served by bridge or device)

---

## Version Progression Analysis

### Early Versions (OLED 1-19)
**Focus**: Basic OLED functionality, WiFi setup, OSC communication

- **OLED 1-10**: Initial development, basic display control
- **OLED 10-15**: WiFi Manager integration, captive portal setup
- **OLED 16-19**: OSC message handling, track name display logic

### Version 20 (OLED20)
**Status**: Stable WiFi-based version
**Key Features**:
- WiFi Manager with captive portal "TDS8+Setup" (pw: tds8setup)
- OSC commands: `/trackname`, `/activetrack`, `/ping`, `/reannounce`
- Web UI at `http://tds8.local`
- mDNS support
- Discovery beacons on port 9000
- IP broadcast to Max for Live
- Ableton connection indicator (dot on Track 8)
- Smart text layout (1 or 2 lines, auto-sizing)
- Network splash screens with setup guides

### Versions 21-23
**Focus**: Refinements to WiFi behavior, UI improvements

### Version 24 (OLED24)
**Status**: Enhanced with tracks editor
**New Features**:
- Web-based tracks editor (edit all 8 track names)
- Persistent track name storage in NVS
- Debug endpoint `/debug` for event tracing
- Improved OSC handling with deferred `/pong` sends
- Flash indicator on Screen 1 for `/ping` activity
- "Forget Wi-Fi" functionality
- Quick-start guide screens
- Ableton connection banner

**Issues Identified**:
- Still WiFi-dependent
- No wired-only mode
- Complex WiFi setup for simple USB use cases

### Version 25 (OLED25_OTA / OLED24_OTA)
**Status**: Added OTA firmware updates
**New Features**:
- OTA update capability via HTTP/HTTPS
- Firmware version tracking (v25)
- Update manifest support
- "Rescue AP" mode (TDS8-Setup) for recovery
- 5-minute timeout on rescue AP

**Issues**:
- Still requires WiFi even for basic operation
- OTA adds complexity

### Version 26 (OLED26_OTA) ⭐ **LAST STABLE WIFI VERSION**
**Status**: Most complete WiFi-based version
**Key Features**:
- All features from v24 + v25
- Stable WiFi Manager integration
- Rescue AP with auto-stop on connection
- OTA updates working
- Full web UI with firmware card
- Tracks editor with save/reload/reset
- Network scanning
- Comprehensive error handling

**Why This Is Important**:
- This is your last known-good WiFi implementation
- All WiFi features working correctly
- Good foundation for adding wired mode

---

## Wired Mode Versions (27-29) ⚠️ **PROBLEMATIC**

### Version 27 (OLED27_OTA_Wired)
**Goal**: Add wired-only mode as default
**Approach**: Minimal implementation
**Features**:
- `wiredOnly` flag (default: true)
- Serial command parser
- Commands: `VERSION`, `WIRED_ONLY`, `WIFI_ON`, `WIFI_JOIN`, `FORGET`, `OTA_URL`
- Web server only active when WiFi connected
- Simplified - **NO OLED DISPLAYS**

**Issues**:
- Removed all OLED functionality
- Removed all track display logic
- Removed OSC handling
- Basically gutted the core functionality
- Only has command parsing shell

### Version 28 (OLED28_OTA_Wired)
**Goal**: Add OLED back with wired-mode intro
**Approach**: Single OLED with rotating intro slides
**Features**:
- OLED intro slides (3 slides rotating every 15s)
- Slide 0: Base info (TDS-8 v1.28, Wired mode, bridge URL)
- Slide 1: USB quick test commands
- Slide 2: WiFi instructions
- `HIDE_INTRO` / `SHOW_INTRO` commands
- Rescue AP (TDS8-Rescue)
- Web dashboard with colorful UI
- Batch track name sending

**Issues**:
- Only ONE OLED display used (not 8)
- No track name display functionality
- No OSC message handling
- No `/trackname` or `/activetrack` implementation
- Intro slides don't help with actual track display
- Lost the core purpose of the device

### Version 29 (OLED29_OTA_Wired_copy)
**Status**: Copy of v28 (timestamp: 2025-10-12 09:15:52)
**Same as v28** - appears to be a backup/working copy

**Critical Issues with v27-29**:
1. **Missing Core Functionality**:
   - No 8-OLED track display
   - No TCA9548A multiplexer usage
   - No track name rendering
   - No active track highlighting
   - No OSC message handling for tracks

2. **Architecture Problems**:
   - Focused on intro/setup instead of operation
   - Single OLED vs. 8 OLEDs
   - No integration with Max for Live
   - Command parsing exists but no action

3. **Incomplete Bridge Integration**:
   - Bridge expects track commands to work
   - Device doesn't implement track logic
   - Disconnect between bridge and firmware

---

## Test Projects

### OLED_Diagnostic
Simple diagnostic tool to test OLED initialization on all 8 channels

### OLED-cylon
Animation test - "Cylon eye" effect bouncing across 8 displays
- Tests I²C multiplexer switching
- Demonstrates slow refresh rate issue with I²C OLEDs
- Confirms animations are not practical

### OLED-running_man
Bouncing ball animation across displays
- Tests continuous display updates
- Simulates bezel gaps between screens
- Further confirms animation limitations

**Conclusion from Tests**: I²C OLED refresh rate is too slow for smooth animations, focus on static/text display

---

## Bridge Project (tds8-bridge)

**Purpose**: Desktop application to relay serial commands and provide web UI

**Features**:
- Serial port detection and connection
- WebSocket for real-time updates
- Communication mode toggle (wired/wifi)
- WiFi network scanning (Windows)
- WiFi join via serial commands
- Track name batch sending
- Active track selection
- OTA firmware updates
- Manifest checking
- Version comparison

**Key Commands Sent**:
- `WIRED_ONLY true/false`
- `WIFI_ON`
- `WIFI_JOIN "ssid" "password"`
- `VERSION`
- `/trackname <index> <name>`
- `/activetrack <index>`
- `/reannounce`
- `OTA_URL <url>`

**Bridge Expectations**:
The bridge assumes the device will:
1. Respond to `VERSION` command
2. Handle track commands and display them
3. Switch between wired/WiFi modes
4. Support OTA updates when in WiFi mode

---

## Recommended Path Forward

### Option 1: Start Fresh from Version 26 ✅ **RECOMMENDED**

**Why Version 26**:
- Last complete, working implementation
- All 8 OLEDs functional
- Track display logic intact
- OSC handling working
- WiFi features stable
- Good code structure

**What to Add**:
1. **Wired Mode Flag**:
   ```cpp
   bool wiredOnly = true;  // default to wired
   bool wifiEnabled = false;
   ```

2. **Serial Command Parser**:
   - Add from v27 (it's good)
   - Keep all existing OSC handlers
   - Route serial commands to same handlers

3. **Dual Communication**:
   ```cpp
   void handleTrackName(int idx, String name) {
     // Same logic, called from OSC or Serial
     trackNames[idx] = name;
     drawTrackName(idx, trackNames[idx]);
   }
   ```

4. **Mode Switching**:
   - Default: WiFi OFF, serial only
   - Command: `WIRED_ONLY false` → enable WiFi
   - Command: `WIFI_ON` → connect to saved network
   - Web dashboard available when WiFi active

5. **Keep All v26 Features**:
   - 8 OLED displays
   - Track name rendering
   - Active track highlighting
   - Web UI (when WiFi on)
   - OTA updates (when WiFi on)
   - Rescue AP
   - All the polish

### Option 2: Fix Version 28/29 ⚠️ **MORE WORK**

**What's Missing**:
1. Add TCA9548A multiplexer support
2. Add 8-OLED initialization
3. Add track name storage (8 strings)
4. Add `drawTrackName()` function from v26
5. Add active track highlighting
6. Implement `/trackname` handler
7. Implement `/activetrack` handler
8. Remove intro slides (or make them optional)

**This is essentially rebuilding v26 into v28**

---

## Technical Details

### I²C Configuration
```cpp
Wire.begin();
Wire.setClock(400000);  // 400kHz fast mode
Wire.setTimeOut(15);    // 15ms timeout
```

### TCA9548A Multiplexer
```cpp
void tcaSelect(uint8_t channel) {
  if (channel > 7) return;
  Wire.beginTransmission(0x70);
  Wire.write(1 << channel);
  Wire.endTransmission();
}
```

### Display Initialization
```cpp
for (uint8_t i = 0; i < 8; i++) {
  tcaSelect(i);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.drawRect(0, 0, 127, 63, SSD1306_WHITE);
  display.display();
}
```

### OSC Commands
- `/ping [replyPort]` → `/pong 1`
- `/trackname <idx> <name>` → Update display
- `/activetrack <idx>` → Highlight display
- `/reannounce` → Broadcast IP

### Serial Commands (v27+)
- `VERSION` → Report firmware version
- `WIRED_ONLY true/false` → Toggle mode
- `WIFI_ON` → Enable WiFi
- `WIFI_JOIN "ssid" "pass"` → Connect to network
- `FORGET` → Clear WiFi credentials
- `OTA_URL <url>` → Update firmware
- `/trackname <idx> <name>` → Same as OSC
- `/activetrack <idx>` → Same as OSC
- `/ping` → `PONG`
- `/reannounce` → Announce

---

## Issues Summary

### Version 29 Problems
1. ❌ Only 1 OLED used (need 8)
2. ❌ No track display functionality
3. ❌ No OSC handlers implemented
4. ❌ Intro slides instead of track info
5. ❌ Bridge sends commands that do nothing
6. ❌ Lost core purpose of device

### What Works in v29
1. ✅ Serial command parsing
2. ✅ WiFi mode switching
3. ✅ Rescue AP
4. ✅ Web dashboard UI
5. ✅ OTA updates
6. ✅ Bridge communication protocol

### What Works in v26
1. ✅ All 8 OLEDs
2. ✅ Track name display
3. ✅ Active track highlighting
4. ✅ OSC communication
5. ✅ WiFi Manager
6. ✅ Web UI
7. ✅ OTA updates
8. ✅ Rescue AP
9. ✅ Track editor
10. ✅ Complete feature set

---

## Recommendation

**Go back to Version 26 and add wired mode support.**

This gives you:
- ✅ All working features
- ✅ Proven stable code
- ✅ Complete OLED implementation
- ✅ Just need to add serial parser
- ✅ Much less work than fixing v29

The serial command parser from v27 is good - just integrate it into v26's existing structure. Keep all the OSC handlers and route serial commands through them.

**Estimated effort**: 2-3 hours vs. 1-2 days to fix v29

---

## Next Steps

1. **Copy OLED26_OTA to OLED30_Wired**
2. **Add serial command parser from v27**
3. **Add `wiredOnly` flag (default true)**
4. **Route serial commands to existing handlers**
5. **Test with bridge**
6. **Iterate**

Would you like me to create Version 30 based on this analysis?
