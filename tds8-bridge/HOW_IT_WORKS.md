# 📖 How Track Names Work

## Understanding the System

Your TDS-8 device **does not have a serial command** to read track names back from the device. This is by design - the device stores track names internally but doesn't expose a "get tracks" command.

## What Each Button Does

### 🟣 **Load Stored Tracks** (formerly "Read Current Tracks")
- **What it does:** Loads track names from the **bridge's memory**
- **Source:** The bridge stores the last track names you sent via "Update All Tracks"
- **Use case:** Restore your last session's track names to the UI

### 🟠 **Update All Tracks**
- **What it does:** Sends track names **to the device** via serial
- **Command sent:** `/trackname 0 Track Name` for each track
- **Use case:** Update the device with new track names

### ⚪ **Reset to Defaults**
- **What it does:** Resets UI fields to "Track 1", "Track 2", etc.
- **Source:** Hardcoded defaults
- **Use case:** Start fresh with default names

---

## The Flow

```
1. You type track names in the UI
2. Click "Update All Tracks"
   ↓
3. Bridge sends to device: /trackname 0 Kick
   ↓
4. Bridge stores in memory: currentTrackNames[0] = "Kick"
   ↓
5. Device updates its display
   ↓
6. Ableton M4L can fetch from bridge: GET /tracks
   ↓
7. You can reload from bridge: Click "Load Stored Tracks"
```

---

## Why "Load Stored Tracks" Not "Read from Device"?

**Device limitation:** Your device firmware doesn't have a command to read track names back via serial.

**Available device commands:**
- `VERSION` - Get firmware version
- `/trackname <idx> <name>` - **SET** track name (write only)
- `/activetrack <idx>` - Highlight a track
- `/ping` - Heartbeat
- `WIFI_JOIN`, `FORGET`, `REBOOT`, etc.

**No read command exists!**

---

## For Ableton Integration

Your M4L patch works perfectly because:
1. Bridge stores track names when you update them
2. M4L fetches from bridge via `GET /tracks`
3. Bridge returns: `{"names": ["Kick", "Snare", ...]}`

**This is the correct workflow!** ✅

---

## Summary

- ✅ **Ableton M4L:** Fetches from bridge via HTTP
- ✅ **Update Tracks:** Sends to device via serial
- ✅ **Load Stored:** Gets from bridge memory
- ❌ **Read from Device:** Not possible (no command exists)

**Everything is working as designed!** 🎉
