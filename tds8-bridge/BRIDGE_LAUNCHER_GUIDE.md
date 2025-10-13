# TDS-8 Bridge Auto-Launcher for Mac

## 🎯 Three Ways to Auto-Launch the Bridge

I've created multiple options for you. Choose the one that fits your workflow best!

---

## Option 1: Simple Shell Script (Easiest)

### What It Does
- Double-click to start bridge
- Shows output in Terminal
- Easy to stop (Ctrl+C)

### Setup (1 minute)

1. **Make script executable:**
   ```bash
   chmod +x /Users/test/Documents/TDS-8/tds8-bridge/bridge-launcher.sh
   ```

2. **Run it:**
   ```bash
   /Users/test/Documents/TDS-8/tds8-bridge/bridge-launcher.sh
   ```
   
   Or just double-click it!
   
   **Note:** The script auto-detects its location, no path configuration needed!

### Add to Dock (Optional)
1. Right-click `bridge-launcher.sh`
2. Open With → Terminal
3. Drag Terminal icon from Dock while running
4. Right-click → Options → Keep in Dock

---

## Option 2: LaunchAgent (Auto-Start on Login)

### What It Does
- Starts automatically when you log in
- Runs in background
- Restarts if it crashes
- Professional setup

### Setup (3 minutes)

1. **Update paths** in `com.tds8.bridge.plist`:
   - Line 10: Path to `bridge-launcher.sh`
   - Line 29: Path to your bridge directory

2. **Make launcher executable:**
   ```bash
   chmod +x /Users/test/Documents/TDS-8/tds8-bridge/bridge-launcher.sh
   ```

3. **Copy to LaunchAgents:**
   ```bash
   cp /Users/test/Documents/TDS-8/tds8-bridge/com.tds8.bridge.plist ~/Library/LaunchAgents/
   ```

4. **Load the service:**
   ```bash
   launchctl load ~/Library/LaunchAgents/com.tds8.bridge.plist
   ```

5. **Start it now:**
   ```bash
   launchctl start com.tds8.bridge
   ```

### Management Commands

**Check if running:**
```bash
launchctl list | grep tds8
```

**Stop the service:**
```bash
launchctl stop com.tds8.bridge
```

**Unload (disable auto-start):**
```bash
launchctl unload ~/Library/LaunchAgents/com.tds8.bridge.plist
```

**View logs:**
```bash
tail -f ~/Documents/TDS-8/bridge-stdout.log
tail -f ~/Documents/TDS-8/bridge-stderr.log
```

---

## Option 3: Automator App (Click to Launch)

### Create a Mac App

1. **Open Automator**
2. **New Document** → **Application**
3. **Add Action**: "Run Shell Script"
4. **Paste this:**
   ```bash
   cd /Users/test/Documents/TDS-8/tds8-bridge
   npm start
   ```
5. **Save as**: `TDS-8 Bridge.app` in Applications
6. **Add to Dock** or Login Items

### Add to Login Items
1. System Preferences → Users & Groups
2. Login Items tab
3. Click "+" and select `TDS-8 Bridge.app`

---

## 🔧 Configuration

### Update Bridge Location

**In `bridge-launcher.sh` (line 6):**
```bash
BRIDGE_DIR="$HOME/Documents/TDS-8/tds8-bridge"
```

**In `com.tds8.bridge.plist` (line 29):**
```xml
<string>/Users/test/Documents/TDS-8/tds8-bridge</string>
```

Change these to match your actual bridge location!

---

## 🧪 Testing

### Test the Shell Script
```bash
cd /Users/test/Documents/TDS-8
./bridge-launcher.sh
```

**Expected output:**
```
🚀 TDS-8 Bridge Launcher
================================
✅ Starting TDS-8 Bridge...
Logs will be written to: /Users/test/Documents/TDS-8/bridge.log

> tds8-bridge@1.0.0 start
> node server.js

TDS-8 Bridge listening on port 3000
Serial port detected: /dev/cu.usbmodem...
```

### Test LaunchAgent
```bash
launchctl start com.tds8.bridge
sleep 2
launchctl list | grep tds8
```

Should show the service running.

---

## 📋 Troubleshooting

### "Permission denied" error
```bash
chmod +x /Users/test/Documents/TDS-8/bridge-launcher.sh
```

### Bridge doesn't start
1. Check bridge directory path is correct
2. Make sure Node.js is installed: `node --version`
3. Check logs: `cat ~/Documents/TDS-8/bridge-stdout.log`

### LaunchAgent not working
1. Check plist syntax: `plutil ~/Library/LaunchAgents/com.tds8.bridge.plist`
2. View system logs: `log show --predicate 'process == "launchd"' --last 5m`
3. Make sure paths are absolute (not relative)

### Can't find npm
Add to plist EnvironmentVariables:
```xml
<key>PATH</key>
<string>/usr/local/bin:/usr/bin:/bin:/opt/homebrew/bin</string>
```

---

## 🎯 Recommended Setup

**For Development:**
- Use **Option 1** (Shell Script)
- Easy to start/stop
- See output immediately

**For Production/Daily Use:**
- Use **Option 2** (LaunchAgent)
- Auto-starts on login
- Runs in background
- Restarts if crashes

**For Non-Technical Users:**
- Use **Option 3** (Automator App)
- Simple double-click to launch
- Can add to Dock
- Looks like a regular app

---

## 📝 Quick Reference

### Start Bridge Manually
```bash
cd /Users/test/Documents/TDS-8/tds8-bridge
npm start
```

### Start with Launcher
```bash
/Users/test/Documents/TDS-8/bridge-launcher.sh
```

### Start LaunchAgent
```bash
launchctl start com.tds8.bridge
```

### View Logs
```bash
tail -f ~/Documents/TDS-8/bridge.log
```

### Stop Everything
```bash
# Stop LaunchAgent
launchctl stop com.tds8.bridge

# Or kill process
pkill -f "tds8-bridge"
```

---

## ✅ Success Checklist

- [ ] Bridge directory path updated in scripts
- [ ] Shell script is executable
- [ ] Can run bridge manually
- [ ] Launcher script works
- [ ] (Optional) LaunchAgent installed
- [ ] (Optional) Added to Login Items
- [ ] Bridge connects to TDS-8
- [ ] Serial commands work through bridge

---

**Choose your preferred method and let me know if you need help setting it up!** 🚀
