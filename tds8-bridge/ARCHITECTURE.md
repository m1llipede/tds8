# 🎵 TDS-8 Bridge Architecture

## Communication Flow

```
┌─────────────┐         ┌──────────────┐         ┌─────────┐
│  Ableton    │         │   Bridge     │         │  TDS-8  │
│  M4L Patch  │         │  (Node.js)   │         │ Device  │
└─────────────┘         └──────────────┘         └─────────┘
       │                       │                       │
       │  1. Receive /ipupdate │                       │
       │◄──────────────────────┤                       │
       │   127.0.0.1:9000      │                       │
       │                       │                       │
       │  2. Send OSC commands │                       │
       ├──────────────────────►│                       │
       │   127.0.0.1:8000      │                       │
       │   /trackname, etc.    │                       │
       │                       │  3. Forward via USB   │
       │                       ├──────────────────────►│
       │                       │   Serial commands     │
       │                       │                       │
       │                       │  4. Device responses  │
       │                       │◄──────────────────────┤
       │                       │   Serial data         │
```

## Port Configuration

### Bridge Ports:
- **HTTP Server:** `8088` - Web UI and REST API
- **OSC Listener:** `8000` - Receives from M4L
- **OSC Sender:** `9001` - Sends to M4L on port 9000
- **WebSocket:** `8088` - Real-time UI updates

### M4L Patch:
- **UDP Receive:** `9000` - Listens for `/ipupdate`
- **UDP Send:** `8000` - Sends commands to bridge

### TDS-8 Device (WiFi mode):
- **OSC Listener:** `8000` - Receives commands
- **OSC Sender:** `9000` - Sends status/feedback
- **HTTP Server:** `80` - Web dashboard, `/tracks` endpoint

## OSC Messages

### Bridge → M4L:
- `/ipupdate 127.0.0.1` - Announces bridge IP address

### M4L → Bridge → Device:
- `/trackname <index> <name>` - Set track name (0-7)
- `/activetrack <index>` - Highlight active track
- `/ping` - Heartbeat check

## Wired Mode (USB)
1. Bridge connects to device via USB serial
2. Bridge broadcasts `/ipupdate 127.0.0.1` to M4L
3. M4L sends OSC to `127.0.0.1:8000`
4. Bridge forwards as serial commands
5. Device responds via serial
6. Bridge logs in UI

## WiFi Mode
1. Device connects to WiFi network
2. Device broadcasts `/ipupdate <device-ip>` to network
3. M4L receives device IP
4. M4L sends OSC directly to `<device-ip>:8000`
5. Device responds via OSC to M4L port 9000

---

**Current Status:** Wired mode fully functional ✅
