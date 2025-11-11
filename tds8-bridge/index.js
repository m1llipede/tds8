#!/usr/bin/env node
// TDS-8 Desktop Bridge — comm toggle (wired|wifi), Wi-Fi scan (Windows), batch tracks, firmware feed + manifest

const path = require('path');
const fs = require('fs');
const express = require('express');
const morgan = require('morgan');
const http = require('http');
const WebSocket = require('ws');
const osc = require('osc');
const { exec, spawn } = require('child_process');
const { SerialPortStream } = require('@serialport/stream');
const { autoDetect } = require('@serialport/bindings-cpp');
const axios = require('axios');
const tmp = require('tmp');

const PORT = process.env.PORT || 8088;
const OSC_LISTEN_PORT = parseInt(process.env.OSC_LISTEN_PORT || '8003', 10);  // Changed to 8003 to avoid blocking
const BAUD = parseInt(process.env.BAUD || '115200', 10);

// Optional defaults (you can set these later)
const OTA_MANIFEST_URL = process.env.OTA_MANIFEST_URL || '';
const FW_FEED_URL = process.env.FW_FEED_URL || '';

const Bindings = autoDetect();

// Multi-device support: Store up to 4 TDS-8 devices
const MAX_DEVICES = 4;
let devices = []; // Array of { id, serial, path, buffer, version, deviceID }
// Track connection attempts and recent closes to avoid reset loops on ESP auto-boot
const connectingPorts = new Set(); // ports currently being opened
const portToDeviceId = new Map();  // sticky mapping of port -> deviceId
const recentlyClosed = new Map();  // port -> timestamp of last close
const RECONNECT_COOLDOWN_MS = 15000; // 15 second cooldown to prevent boot loop reconnects // wait before re-opening after a close

// Global-level dedup keyed by actual track number (0..31)
const lastGlobalTrack = new Map(); // key: globalIndex → { name, ts }
const GLOBAL_TRACK_DEDUP_MS = 4000;
function shouldForwardGlobal(globalIndex, nameEscaped) {
  try {
    const rec = lastGlobalTrack.get(globalIndex);
    const now = Date.now();
    if (rec && rec.name === nameEscaped && (now - rec.ts) < GLOBAL_TRACK_DEDUP_MS) {
      return false;
    }
    lastGlobalTrack.set(globalIndex, { name: nameEscaped, ts: now });
    return true;
  } catch { return true; }
}

// Debounce /reannounce so M4L doesn't get spammed
let lastReannounceTs = 0;
function requestReannounce() {
  const now = Date.now();
  if (now - lastReannounceTs < 2000) return; // 2s gate
  lastReannounceTs = now;
  try { sendOSC('/reannounce', []); } catch {}
}

// Legacy single device variables (for backward compatibility)
let serial = null;
let serialPath = null;
let serialBuf = '';
let deviceVersion = null;

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// OSC UDP Port for broadcasting to Ableton M4L (lazy init)
let udpPort = null;

// De-duplication for /trackname forwarding
// - Content-level: only forward if text changed for (deviceId, localIndex)
// - Burst-level: also suppress identical repeats within a short window
const lastTracknameValue = new Map(); // key: `${deviceId}:${localIndex}` → lastEscapedName
const lastTracknameTime = new Map();  // key: `${deviceId}:${localIndex}:${name}` → timestamp
const TRACKNAME_DEDUP_MS = 4000;
function shouldSendTrackname(deviceId, localIndex, nameEscaped) {
  try {
    const keySlot = `${deviceId}:${localIndex}`;
    const keyBurst = `${deviceId}:${localIndex}:${nameEscaped}`;
    const now = Date.now();
    const prevValue = lastTracknameValue.get(keySlot);
    if (prevValue === nameEscaped) {
      // Same content already sent for this slot → skip
      return false;
    }
    const prevTime = lastTracknameTime.get(keyBurst) || 0;
    if (now - prevTime < TRACKNAME_DEDUP_MS) {
      // Burst duplicate of the exact same payload → skip
      return false;
    }
    // Accept and record
    lastTracknameValue.set(keySlot, nameEscaped);
    lastTracknameTime.set(keyBurst, now);
    return true;
  } catch {
    return true;
  }
}

function normalizeName(s) {
  try {
    return String(s ?? '')
      .replace(/\\"/g, '"')
      .replace(/\s+/g, ' ')
      .trim()
      .toLowerCase();
  } catch { return ''; }
}

function initOSC() {
    if (!udpPort) {
        try {
            udpPort = new osc.UDPPort({
                localAddress: "0.0.0.0",
                localPort: 9001,
                broadcast: true,
                metadata: true
            });
            
            udpPort.on("ready", () => {
                console.log(`📡 OSC Sender: Ready on port 9001 (Bridge → M4L ${M4L_IP}:${M4L_PORT})`);
            });
            
            udpPort.on("error", (err) => {
                console.error("❌ OSC Sender error:", err.message);
            });
            
            udpPort.open();
        } catch (err) {
            console.error("❌ OSC init error:", err.message);
        }
    }
    return udpPort;
}

// Track device IP for WiFi mode (for display/status only)
let deviceIP = "127.0.0.1"; // Default to localhost for wired mode

// M4L always runs on localhost
const M4L_IP = "127.0.0.1";
const M4L_PORT = 8001;  // Changed from 9000 to avoid conflicts

// Ableton connection tracking (module scope for API access)
let abletonConnected = false;
let abletonGreetingSent = false; // Flag to ensure one-time message
let lastHiTime = 0;
let lastHelloResponse = 0;
let reannounceTimer = null; // debounce timer for /reannounce after device connects
let expectedDeviceCount = 0; // target count from last scan
let batchReannounceDone = false; // ensure single /reannounce per scan batch

// Send OSC message to Ableton (M4L)
function sendOSC(address, args = []) {
    console.log(`[sendOSC] Called with address: ${address} and args: ${JSON.stringify(args)}`);
    const port = initOSC();
    if (!port) {
        console.error('❌ OSC port not initialized - cannot send', address);
        return;
    }
    
    if (!port.socket || !port.socket._handle) {
        console.error('❌ OSC socket not ready - cannot send', address);
        return;
    }
    
    try {
        // Send to M4L (always localhost)
        port.send({
            address: address,
            args: args
        }, M4L_IP, M4L_PORT);
        const argsStr = args.length > 0 ? ' [' + args.map(a => a.value || a).join(', ') + ']' : '';
        console.log(`[sendOSC] Successfully sent OSC message: ${address} with args: ${JSON.stringify(args)}`);
        console.log(`📤 OSC → M4L (${M4L_IP}:${M4L_PORT}): ${address}${argsStr}`);
        
        // Broadcast to browser UI
        wsBroadcast({ 
            type: 'osc-sent', 
            address: address, 
            args: args,
            argsStr: argsStr
        });
    } catch (err) {
        console.error(`[sendOSC] Error sending OSC message: ${address} with args: ${JSON.stringify(args)}`, err.message, err.stack);
        console.error(`❌ OSC send error (${address}):`, err.message, err.stack);
    }
}

// OSC Listener on port 8000 for receiving commands from M4L
let oscListener = null;

function initOSCListener() {
    if (!oscListener) {
        try {
            oscListener = new osc.UDPPort({
                localAddress: "0.0.0.0",
                localPort: OSC_LISTEN_PORT,
                metadata: true
            });
            
            // Simple handshake: send /hello, expect /hi back
            // (variables are in module scope)
            
            // Function to check connection and send hello
            const checkConnection = () => {
                // Send /hello
                const now = Date.now();
                const timeSinceLastHi = lastHiTime ? Math.floor((now - lastHiTime) / 1000) : null;
                console.log(`>>> Handshake check... (last /hi: ${timeSinceLastHi ? timeSinceLastHi + 's ago' : 'never'})`);
                sendOSC('/hello', []);
                
                // Check if we got /hi in last 30 seconds
                if (lastHiTime && (now - lastHiTime < 30000)) {
                    if (!abletonGreetingSent) {
                        console.log('✅ Ableton connection confirmed via /hi handshake');
                        sendToAll(`ALERT "Ableton Connected!" 1000\n`);
                        requestReannounce();
                        abletonGreetingSent = true;
                    }
                    abletonConnected = true;
                    wsBroadcast({ type: 'ableton-connected' });
                } else {
                    if (abletonConnected) {
                        abletonConnected = false;
                        abletonGreetingSent = false; // allow re-announce on next handshake
                        console.log('⚠️ Ableton disconnected (no /hi in 30s)');
                        wsBroadcast({ type: 'ableton-disconnected' });
                    }
                }
            };
            
            // Send first hello quickly after startup
            setTimeout(() => {
                console.log('🔔 Starting Ableton handshake...');
                checkConnection();
            }, 300);
            
            // Then send /hello every 10 seconds
            setInterval(checkConnection, 10000);
            
            // Log raw packets for debugging
            oscListener.on("raw", (data, info) => {
                console.log(`📦 RAW OSC packet received from ${info.address}:${info.port}, ${data.length} bytes`);
            });
            
            oscListener.on("message", (oscMsg, timeTag, info) => {
                const fromIP = info.address || 'unknown';
                const fromPort = info.port || 'unknown';
                
                // Format args for display
                const argsStr = oscMsg.args && oscMsg.args.length > 0 
                    ? oscMsg.args.map(a => a.value).join(', ') 
                    : '';
                console.log(`📨 Received OSC: ${oscMsg.address}${argsStr ? ' [' + argsStr + ']' : ''} from ${fromIP}:${fromPort}`);
                
                wsBroadcast({ type: 'osc-received', from: `${fromIP}:${fromPort}`, address: oscMsg.address, args: oscMsg.args });
                
                
                // Handle /hi response to our /hello
                if (oscMsg.address === "/hi") {
                    lastHiTime = Date.now();
                    lastHelloResponse = Date.now();
                    const wasConnected = abletonConnected;
                    abletonConnected = true;
                    
                    console.log(`✓ Received /hi from M4L (port ${OSC_LISTEN_PORT})`);
                    
                    // Send /ableton_on EVERY time we get /hi (triggers heartbeat flash)
                    try {
                        if (serial && serial.isOpen) send('/ableton_on\n');
                    } catch (err) {}
                    
                    if (!wasConnected) {
                        console.log('✓ Ableton CONNECTED');
                        
                        // Send /reannounce to M4L via OSC to request track names
                        setTimeout(() => { requestReannounce(); }, 1000);
                    }
                    return;
                }
                
                // Forward OSC commands to device via serial
                if (oscMsg.address === "/trackname" && oscMsg.args.length >= 2) { // Handle 2 or 3 args
                    // If no serial connection is open yet, ignore quietly to avoid spam on boot
                    const serialReady = (serial && serial.isOpen) || (devices && devices.length > 0);
                    if (!serialReady) {
                        // Silently ignore to prevent console spam on boot before devices are connected
                        return;
                    }
                    const displayIndex = oscMsg.args[0].value;
                    const nameRaw = oscMsg.args[1].value;
                    // Handle both 2-arg (index, name) and 3-arg (index, name, actualTrack) messages
                    const actualTrack = oscMsg.args.length >= 3 ? oscMsg.args[2].value : displayIndex;

                    // Sanitize name: strip outer quotes if present (M4L may include them), then escape inner quotes
                    let nameStr = String(nameRaw).trim();
                    if (nameStr.length >= 2 && nameStr.startsWith('"') && nameStr.endsWith('"')) {
                      nameStr = nameStr.slice(1, -1);
                    }
                    const esc = nameStr.replace(/"/g, '\\"');
                    const escNorm = normalizeName(nameStr);
                    const globalIndex = Number.isFinite(actualTrack) ? Number(actualTrack) : Number(displayIndex);
                    if (!shouldForwardGlobal(globalIndex, escNorm)) {
                      console.log(`⏭️  Deduped global /trackname for track ${globalIndex}`);
                      return;
                    }
                    
                    // Multi-device: Route to correct device based on track number
                    const deviceId = Math.floor(actualTrack / 8);
                    const localIndex = actualTrack % 8;
                    const targetDevice = devices.find(d => d.id === deviceId);

                    if (targetDevice) {
                        if (shouldSendTrackname(deviceId, localIndex, escNorm)) {
                          const cmd = `/trackname ${localIndex} "${esc}" ${actualTrack}\n`;
                          const delay = Math.max(0, (Number.isFinite(localIndex) ? localIndex : 0) * 100);
                          setTimeout(() => sendToDevice(deviceId, cmd), delay);
                          console.log(`📍 Routed track ${actualTrack} to Device ${deviceId} (local index ${localIndex}, delay ${delay}ms)`);
                        } else {
                          console.log(`⏭️  Deduped /trackname for Device ${deviceId} idx ${localIndex}`);
                        }
                    } else if (devices.length > 0) {
                        // Fallback for single connected device if routing fails
                        const fallbackId = devices[0].id;
                        if (shouldSendTrackname(fallbackId, displayIndex, escNorm)) {
                          const cmd = `/trackname ${displayIndex} "${esc}" ${actualTrack}\n`;
                          const delay = Math.max(0, (Number.isFinite(displayIndex) ? displayIndex : 0) * 100);
                          setTimeout(() => sendToDevice(fallbackId, cmd), delay);
                        } else {
                          console.log(`⏭️  Deduped /trackname for fallback Device ${fallbackId} idx ${displayIndex}`);
                        }
                    } else {
                        // Single device mode (legacy)
                        if (serial && serial.isOpen) {
                            if (shouldSendTrackname(0, displayIndex, escNorm)) {
                              const cmd = `/trackname ${displayIndex} "${esc}" ${actualTrack}\n`;
                              const delay = Math.max(0, (Number.isFinite(displayIndex) ? displayIndex : 0) * 100);
                              setTimeout(() => send(cmd), delay);
                            } else {
                              console.log(`⏭️  Deduped /trackname for legacy serial idx ${displayIndex}`);
                            }
                        } else {
                            // Silently ignore if the single-device serial port isn't open
                        }
                    }

                    // Store the UNESCAPED name for UI/API (limited to first 8 fields)
                    if (Number.isFinite(globalIndex) && globalIndex >= 0 && globalIndex < 8) {
                        currentTrackNames[globalIndex] = String(nameRaw);
                    }
                }
                else if (oscMsg.address === "/activetrack" && oscMsg.args.length >= 1) {
                    const index = oscMsg.args[0].value;
                    const deviceId = Math.floor(index / 8);
                    const localIndex = index % 8;
                    if (devices && devices.length > 0) {
                    const targetDevice = devices.find(d => d.id === deviceId);
                    if (targetDevice) {
                        // Send selection to target device
                        sendToDevice(deviceId, `/activetrack ${localIndex}\n`);
                        // Clear selection on all other devices
                        devices.filter(d => d.id !== deviceId && d.serial && d.serial.isOpen)
                               .forEach(d => sendToDevice(d.id, `/activetrack -1\n`));
                    } else if (devices.length === 1) {
                        sendToDevice(devices[0].id, `/activetrack ${localIndex}\n`);
                    } else {
                        // No exact match; conservatively clear all then no-op (or choose first)
                        sendToAll(`/activetrack -1\n`);
                    }
                    } else {
                    send(`/activetrack ${index}\n`);
                    }
                }
                else if (oscMsg.address === "/ping") {
                    send(`/ping\n`);
                }
            });
            
            oscListener.on("error", (err) => {
                console.error("OSC listener error:", err.message);
            });
            
            oscListener.on("ready", () => {
                console.log(`🎧 OSC Listener: Ready on port ${OSC_LISTEN_PORT} (receives from Ableton)`);
            });
            
            oscListener.open();
        } catch (err) {
            console.error("OSC listener init error:", err.message);
        }
    }
    return oscListener;
}

// Start OSC sender and listener immediately
console.log('\n========== OSC INITIALIZATION ==========');
console.log('Calling initOSC()...');
const oscSender = initOSC();        // Initialize OSC sender
console.log('OSC Sender returned:', oscSender ? 'object' : 'null');

console.log('Calling initOSCListener()...');
const oscReceiver = initOSCListener(); // Initialize OSC listener (port 8000 - from M4L)
console.log('OSC Listener returned:', oscReceiver ? 'object' : 'null');

// Wait a moment for ports to open, then verify
setTimeout(() => {
  console.log('\n🔍 OSC Status Check:');
  console.log(`  Sender (port 9001): ${oscSender && oscSender.socket ? '✅ Ready' : '❌ Failed'}`);
  console.log(`  Listener (port ${OSC_LISTEN_PORT}): ${oscReceiver && oscReceiver.socket ? '✅ Ready' : '❌ Failed'}`);
  
  if (!oscSender || !oscSender.socket) {
    console.error('⚠️ OSC Sender failed to initialize - M4L communication will not work!');
    console.error('   Check if port 9001 is already in use by another application.');
  }
  if (!oscReceiver || !oscReceiver.socket) {
    console.error('⚠️ OSC Listener failed to initialize - Cannot receive from M4L!');
    console.error(`   Check if port ${OSC_LISTEN_PORT} is already in use by another application.`);
  }
  
  // Test send
  console.log('\n🧪 Sending test /hello to M4L...');
  if (oscSender && oscSender.socket) {
    try {
      oscSender.send({ address: '/hello', args: [] }, M4L_IP, M4L_PORT);
      console.log(`✅ Test message sent successfully to ${M4L_IP}:${M4L_PORT}`);
    } catch (e) {
      console.error('❌ Test send failed:', e.message);
    }
  }
}, 1000);

console.log('========================================\n');

app.use(express.json({ limit: '10mb' }));
app.use(morgan('dev'));
app.use(express.static(path.join(__dirname, 'web')));

app.post('/api/firmware/flash', async (req, res) => {
  const { devicePath, firmwareUrl } = req.body;
  if (!devicePath || !firmwareUrl) {
    return res.status(400).json({ ok: false, error: 'Missing device path or firmware URL.' });
  }

  const device = devices.find(d => d.path === devicePath);
  if (!device) {
    return res.status(404).json({ ok: false, error: 'Device not found or not connected.' });
  }

  // 1. Disconnect the device to free up the serial port
  uiLog(`[FLASH] Disconnecting ${devicePath} to begin flashing...`);
  try {
    if (device.serial && device.serial.isOpen) {
      await new Promise(resolve => device.serial.close(resolve));
    }
    // Remove from our active list
    devices = devices.filter(d => d.path !== devicePath);
    wsBroadcast({ type: 'serial-close', path: devicePath }); // Notify UI
    uiLog(`[FLASH] Port ${devicePath} is now free.`);
  } catch (e) {
    console.error(`[FLASH] Error closing port ${devicePath}:`, e);
    return res.status(500).json({ ok: false, error: `Failed to close serial port: ${e.message}` });
  }

  // 2. Download firmware to a temporary file
  let tmpFile;
  try {
    uiLog(`[FLASH] Downloading firmware from ${firmwareUrl}...`);
    const response = await axios.get(firmwareUrl, { responseType: 'arraybuffer' });
    tmpFile = tmp.fileSync({ postfix: '.bin' });
    fs.writeFileSync(tmpFile.name, response.data);
    uiLog(`[FLASH] Firmware saved to temporary file: ${tmpFile.name}`);
  } catch (e) {
    console.error(`[FLASH] Firmware download failed:`, e);
    if (tmpFile) tmpFile.removeCallback();
    return res.status(500).json({ ok: false, error: `Firmware download failed: ${e.message}` });
  }

  // 3. Execute esptool.py
  const esptoolCommand = `python -m esptool --chip esp32c3 --port ${devicePath} --baud 115200 write_flash -z 0x0 ${tmpFile.name}`;
  uiLog(`[FLASH] Executing: ${esptoolCommand}`);
  const child = exec(esptoolCommand);

  child.stdout.on('data', (data) => {
    const output = data.toString();
    console.log(`[esptool]: ${output.trim()}`);
    uiLog(`[esptool] ${output.trim()}`);
  });
  child.stderr.on('data', (data) => {
    const output = data.toString();
    console.error(`[esptool]: ${output.trim()}`);
    uiLog(`[esptool] ${output.trim()}`);
  });

  child.on('close', (code) => {
    uiLog(`[FLASH] esptool.py process finished with exit code ${code}.`);
    tmpFile.removeCallback(); // Clean up the temporary file

    if (code === 0) {
      uiLog(`[FLASH] Firmware update successful! Device will reboot.`);
      res.json({ ok: true, message: 'Flash successful!' });
    } else {
      uiLog(`[FLASH] esptool.py failed. Please check the log.`);
      res.status(500).json({ ok: false, error: 'esptool.py failed. See log for details.' });
    }
    // The frontend's periodic scan will pick up the device after it reboots.
  });
});

app.get('/api/firmware/releases', async (req, res) => {
  try {
    const response = await axios.get('https://api.github.com/repos/m1llipede/tds8/releases');
    const releases = response.data.map(release => {
      return {
        name: release.name,
        tag_name: release.tag_name,
        assets: release.assets.filter(asset => asset.name.endsWith('.bin')).map(asset => {
          return {
            name: asset.name,
            browser_download_url: asset.browser_download_url
          }
        })
      }
    }).filter(release => release.assets.length > 0);
    res.json({ ok: true, releases });
  } catch (error) {
    console.error('Error fetching GitHub releases:', error.message);
    res.status(500).json({ ok: false, error: 'Failed to fetch releases from GitHub' });
  }
});

// API endpoint to fetch firmware releases from GitHub

// Serve index.html at root
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'web', 'index.html'));
});

// Redirect old favicon.ico requests to favicon.svg
app.get('/favicon.ico', (req, res) => {
  res.redirect(301, '/favicon.svg');
});

function wsBroadcast(obj) {
  const s = JSON.stringify(obj);
  for (const c of wss.clients) if (c.readyState === 1) c.send(s);
}

// Helper: print a line to the Serial Monitor area in the web UI
function uiLog(line) {
  try { wsBroadcast({ type: 'serial-data', data: line }); } catch {}
}

// -------- Serial helpers --------
async function listPorts() {
  try { return await Bindings.list(); }
  catch { return []; }
}

async function openSerial(desiredPath) {
  if (serial && serial.isOpen && desiredPath && desiredPath !== serialPath) {
    try { serial.close(); } catch {}
    serial = null; serialPath = null;
  }
  if (serial && serial.isOpen) return serial.path;
  if (!desiredPath) throw new Error('No port specified. Select a serial port first.');

  serialPath = desiredPath;
  serial = new SerialPortStream({
    binding: Bindings, 
    path: desiredPath, 
    baudRate: BAUD, 
    autoOpen: false
  });
  
  await new Promise((res, rej) => {
    serial.open((err) => {
      if (err) return rej(err);
      // Wait 1 second for device to stabilize after open
      setTimeout(() => res(), 1000);
    });
  });

  serial.on('data', onSerialData);
  serial.on('error', e => wsBroadcast({ type: 'serial-error', message: e.message }));
  serial.on('close', () => wsBroadcast({ type: 'serial-close', path: serialPath }));

  wsBroadcast({ type: 'serial-open', path: serialPath, baud: BAUD });
  console.log(`Serial Port: ${serialPath} @ ${BAUD} baud`);
  
  // Wait 3 seconds for device to finish booting before sending commands
  setTimeout(() => {
    if (serial && serial.isOpen) {
      send('/reannounce\n'); 
      send('VERSION\n');
    }
  }, 3000);
  return serialPath;
}

function send(s) {
  if (!serial || !serial.isOpen) {
    console.error('❌ Cannot send - Serial not open');
    throw new Error('Serial not open');
  }
  serial.write(s);
  console.log(`SENT: ${s.trim()}`);
  uiLog(`[SERIAL] SENT: ${s.trim()}`);
}

// Multi-device: Send to specific device by ID
function sendToDevice(deviceId, s) {
  console.log(`🔍 [sendToDevice] Attempting to send to deviceId=${deviceId}`);
  console.log(`🔍 [sendToDevice] Current devices array:`, devices.map(d => `[id=${d.id}, path=${d.path}, serial=${d.serial ? 'open' : 'null'}]`).join(', '));
  
  const device = devices.find(d => d.id === deviceId);
  if (!device) {
    console.error(`❌ Device with id=${deviceId} not found in devices array`);
    throw new Error(`Device ${deviceId} not found`);
  }
  if (!device.serial) {
    console.error(`❌ Device ${deviceId} has no serial object`);
    throw new Error(`Device ${deviceId} has no serial connection`);
  }
  if (!device.serial.isOpen) {
    console.error(`❌ Device ${deviceId} serial port is not open`);
    throw new Error(`Device ${deviceId} serial port closed`);
  }
  
  device.serial.write(s);
  console.log(`✅ SENT to Device ${deviceId} (${device.path}): ${s.trim()}`);
  uiLog(`[Device ${deviceId}] SENT: ${s.trim()}`);
}

// Multi-device: Send to all connected devices
function sendToAll(s) {
  devices.forEach(device => {
    if (device.serial && device.serial.isOpen) {
      device.serial.write(s);
      console.log(`SENT to Device ${device.id}: ${s.trim()}`);
      uiLog(`[Device ${device.id}] SENT: ${s.trim()}`);
    }
  });
}

// Debounce sending /reannounce so we issue it once after a burst of device connections
function scheduleReannounce(delay = 4000) {
  try {
    if (reannounceTimer) clearTimeout(reannounceTimer);
    reannounceTimer = setTimeout(() => {
      try { requestReannounce(); } catch (e) {}
    }, delay);
  } catch {}
}

function maybeReannounceBatch() {
  try {
    const connectedCount = devices.length;
    if (batchReannounceDone) return;
    // If we met or exceeded the expected count for this scan, announce immediately
    if (expectedDeviceCount > 0 && connectedCount >= expectedDeviceCount) {
      batchReannounceDone = true;
      requestReannounce();
      return;
    }
    // Fallback: on smaller setups or when some ports are cooling down, announce once
    // as soon as at least one device is connected. Use debounce to avoid duplicates.
    if (connectedCount >= 1) {
      batchReannounceDone = true;
      scheduleReannounce(3000);
    }
  } catch {}
}

// Multi-device: Handle serial data from specific device
function onSerialDataMulti(device, chunk) {
  try {
  const text = chunk.toString('utf8');
  device.buffer += text;
  wsBroadcast({ type: 'serial-data', deviceId: device.id, data: text });
  let idx;
  while ((idx = device.buffer.indexOf('\n')) >= 0) {
    const line = device.buffer.slice(0, idx).trim();
    device.buffer = device.buffer.slice(idx + 1);
    if (line) {
      console.log(`RECV from Device ${device.id}: ${line}`);
      
      // Parse DEVICE_ID response
      const deviceIdMatch = /^DEVICE_ID\s*[:=]\s*(\d+)/i.exec(line);
      if (deviceIdMatch) {
        device.deviceID = parseInt(deviceIdMatch[1]);
        console.log(`✓ Device ${device.id} confirmed DEVICE_ID: ${device.deviceID}`);
      }
      
      // Parse VERSION response
      const versionMatch = /^VERSION\s*[:=]\s*(.+)/i.exec(line);
      if (versionMatch) {
        device.version = versionMatch[1];
        console.log(`✓ Device ${device.id} version: ${device.version}`);
        wsBroadcast({ type: 'device-version', version: device.version, path: device.path, deviceId: device.id });
      }
    }
  }
} catch (e) {
  console.error(`CRITICAL: onSerialDataMulti error for device ${device.id}:`, e);
  uiLog(`FATAL: Serial data handler failed for device ${device.id}: ${e.message}`);
}
}

function onSerialData(chunk) {
  const text = chunk.toString('utf8');
  serialBuf += text;
  let idx;
  while ((idx = serialBuf.indexOf('\n')) >= 0) {
    const line = serialBuf.slice(0, idx).trim();
    serialBuf = serialBuf.slice(idx + 1);
    const trimmedLine = line.trim();
    if (trimmedLine) {
      // Only broadcast clean lines, not raw buffer data
      wsBroadcast({ type: 'serial-data', data: `Received: ${trimmedLine}` });
      console.log(`RECV: ${trimmedLine}`);
    }
    const m = /^VERSION\s*[:=]\s*([\w.\-]+)/i.exec(line);
    if (m) { deviceVersion = m[1]; wsBroadcast({ type: 'device-version', version: deviceVersion }); }
    
    // Parse MODE: WIRED or MODE: WIFI
    const modeMatch = /^MODE\s*[:=]\s*(WIRED|WIFI)/i.exec(line);
    if (modeMatch) {
      const mode = modeMatch[1].toUpperCase();
      const connectionType = mode === 'WIRED' ? 'wired' : 'wifi';
      if (mode === 'WIRED') {
        deviceIP = '127.0.0.1';
      }
      // For WiFi mode, keep existing deviceIP or wait for /ipupdate
      wsBroadcast({ 
        type: 'device-mode', 
        mode: connectionType,
        ip: mode === 'WIRED' ? '127.0.0.1' : (deviceIP || 'Connecting...')
      });
      // Transparency log to UI when mode line seen over serial
      uiLog(`MODE: ${mode}`);
      const ipNow = mode === 'WIRED' ? '127.0.0.1' : (deviceIP || 'pending…');
      uiLog(`IP: ${ipNow}`);
      uiLog(`OSC: Bridge listening ${OSC_LISTEN_PORT} (from M4L), sending ${M4L_PORT} (to M4L)`);
    }
  }
}

// -------- API --------
app.get('/api/ports', async (_req, res) => {
  const ports = await listPorts();
  console.log('--- UNFILTERED PORT DETAILS (from /api/ports) ---');
  ports.forEach(p => console.log(JSON.stringify(p, null, 2)));
  console.log('-------------------------------------------------');
  console.log('🔍 All serial ports found:', ports.map(p => ({
    path: p.path,
    vid: p.vendorId,
    pid: p.productId,
    manufacturer: p.manufacturer
  })));
  
  // Filter for TDS-8 devices (XIAO ESP32-C3 preferred)
  const tds8Devices = ports.filter(p => {
    const vid = (p.vendorId || '').toUpperCase();
    const pid = (p.productId || '').toUpperCase();
    const manu = (p.manufacturer || '').toLowerCase();
    // Match specific VID/PID for ESP32-C3, or common Arduino names
    return (vid === '303A' && pid === '1001') || manu.includes('arduino') || manu.includes('tds-8');
  });

  let list = tds8Devices;
  if (list.length === 0) {
    // Fallback: return all ports so user can choose manually
    console.log('⚠️ No VID/PID match; falling back to all serial ports');
    list = ports;
    // On Windows, if still empty, try PowerShell discovery of COM ports
    if (list.length === 0 && process.platform === 'win32') {
      try {
        await new Promise((resolve) => {
          const cmd = 'powershell -NoProfile -Command "try { Get-CimInstance Win32_SerialPort | Select-Object -ExpandProperty DeviceID } catch { try { Get-WmiObject Win32_SerialPort | Select-Object -ExpandProperty DeviceID } catch {} }"';
          exec(cmd, { windowsHide: true }, (err, stdout) => {
            if (!err && stdout) {
              const lines = stdout.split(/\r?\n/).map(s => s.trim()).filter(Boolean);
              const extra = lines.filter(s => /^COM\d+$/i.test(s)).map(id => ({ path: id }));
              if (extra.length > 0) {
                console.log('➕ Discovered COM ports via PowerShell:', extra.map(e => e.path));
                list = extra;
              }
            }
            resolve();
          });
        });
      } catch (e) {
        console.log('PowerShell COM discovery skipped:', e.message);
      }
    }
  } else {
    console.log(`✓ Filtered devices (VID=303A, PID=1001): ${tds8Devices.length} found`);
  }

  const decorated = list.map(p => ({
    path: p.path,
    label: [p.manufacturer, p.path].filter(Boolean).join(' - ') || p.path,
    vendorId: p.vendorId || '',
    productId: p.productId || '',
    serialNumber: p.serialNumber || '',
    manufacturer: p.manufacturer || '',
    pnpId: p.pnpId || '',
    isTDS8: tds8Devices.some(t => t.path === p.path)
  }));
  res.json(decorated);
});

// New endpoint to get ALL serial ports (unfiltered) for alternate connection dropdown
app.get('/api/ports/all', async (_req, res) => {
  const ports = await listPorts();
  const psPortInfo = new Map(); // Store PowerShell data by DeviceID
  
  // On Windows, get detailed port info from PowerShell
  if (process.platform === 'win32') {
    try {
      const discoveredPorts = await new Promise((resolve) => {
        const cmd = 'powershell -NoProfile -Command "Get-WmiObject Win32_SerialPort | Select-Object DeviceID, Name, Description | ConvertTo-Json"';
        exec(cmd, { windowsHide: true, timeout: 5000 }, (err, stdout) => {
          if (err || !stdout) {
            console.log('PowerShell command failed or returned no output. Using basic list.');
            return resolve([]); // Resolve with empty array on error
          }
          try {
            const psData = JSON.parse(stdout.trim());
            const psArray = Array.isArray(psData) ? psData : [psData];
            resolve(psArray.filter(p => p.DeviceID)); // Filter out invalid entries
          } catch (e) {
            console.log('PowerShell JSON parse error:', e.message);
            resolve([]); // Resolve with empty array on parse error
          }
        });
      });

      // Add discovered ports to the main list if they aren't already there
      discoveredPorts.forEach(psPort => {
        psPortInfo.set(psPort.DeviceID, { name: psPort.Name, description: psPort.Description });
        if (!ports.some(p => p.path === psPort.DeviceID)) {
          ports.push({
            path: psPort.DeviceID,
            manufacturer: psPort.Name || psPort.Description || '',
            pnpId: psPort.Description || ''
          });
        }
      });

    } catch (e) {
      console.log('PowerShell COM discovery completely failed:', e.message);
    }
  }
  
  const decorated = ports.map(p => {
    let displayLabel = p.path;
    
    // Prefer PowerShell Name over manufacturer
    const psInfo = psPortInfo.get(p.path);
    if (psInfo && psInfo.name) {
      displayLabel = psInfo.name;
      console.log(`✓ Using PowerShell name for ${p.path}: ${displayLabel}`);
    } else if (p.manufacturer && p.manufacturer !== 'Microsoft') {
      displayLabel = `${p.manufacturer} - ${p.path}`;
      console.log(`Using manufacturer for ${p.path}: ${displayLabel}`);
    } else {
      console.log(`⚠️ No good label for ${p.path}, using path only. Manufacturer: ${p.manufacturer}`);
    }
    
    return {
      path: p.path,
      label: displayLabel,
      vendorId: p.vendorId || '',
      productId: p.productId || '',
      serialNumber: p.serialNumber || '',
      manufacturer: p.manufacturer || '',
      pnpId: p.pnpId || ''
    };
  });
  
  const filteredPorts = decorated.filter(p => {
    const mfg = (p.manufacturer || '').toLowerCase();
    const lbl = (p.label || '').toLowerCase();
    const isTDS8 = 
      mfg.includes('arduino') || 
      mfg.includes('seeed') || 
      lbl.includes('tds-8') || 
      p.vendorId === '2886' || // Seeed Studio
      p.vendorId === '2341' || // Arduino
      p.vendorId === '1a86';   // Common CH340 serial chip

    if (isTDS8) console.log(`✓ Found potential TDS-8: ${p.label}`);
    return isTDS8;
  });

  console.log(`✅ Filtered to ${filteredPorts.length} potential TDS-8 device(s)`);
  res.json(filteredPorts);
});

// Map COM ports to device IDs (persistent across disconnects)

app.post('/api/connect', async (req, res) => {
  try {
    const desired = req.body && req.body.path;
    const requestedDeviceId = req.body && req.body.deviceId; // Frontend can specify device ID
    if (!desired) throw new Error('Missing path');
    
    // Check if device is already connected
    const existingDevice = devices.find(d => d.path === desired);
    if (existingDevice) {
      console.log(`⚠️ Device ${desired} already connected as Device ID ${existingDevice.deviceID}`);
      return res.json({ ok: true, path: desired, baud: BAUD, deviceId: existingDevice.deviceID, alreadyConnected: true });
    }
    
    // Determine device ID: use requested ID, or check port mapping, or assign next available
    let deviceId;
    if (requestedDeviceId !== undefined) {
      deviceId = requestedDeviceId;
      portToDeviceId.set(desired, deviceId);
    } else if (portToDeviceId.has(desired)) {
      // This port has been used before - restore its ID
      deviceId = portToDeviceId.get(desired);
      console.log(`📌 Restoring Device ID ${deviceId} for ${desired} (previously assigned)`);
    } else {
      // New port - assign next available ID (0-3)
      const usedIds = new Set(Array.from(portToDeviceId.values()));
      for (let i = 0; i < MAX_DEVICES; i++) {
        if (!usedIds.has(i)) {
          deviceId = i;
          break;
        }
      }
      if (deviceId === undefined) deviceId = 0; // Fallback
      portToDeviceId.set(desired, deviceId);
      console.log(`🆕 Assigning Device ID ${deviceId} to ${desired} (new port)`);
    }
    
    // Use consistent multi-device pattern for all devices (including first)
    const newSerial = new SerialPortStream({ 
      binding: Bindings, 
      path: desired, 
      baudRate: BAUD,
      autoOpen: false
    });
    
    const device = {
      id: deviceId,
      serial: newSerial,
      path: desired,
      buffer: '',
      version: null,
      deviceID: deviceId
    };
    
    // Open port with minimal intervention
    await new Promise((resolve, reject) => {
      newSerial.open((err) => {
        if (err) return reject(err);
        // Wait 1 second for device to stabilize
        setTimeout(() => resolve(), 1000);
      });
    });
    
    devices.push(device);
    
    console.log(`📊 [DEVICES] Array after connection:`, devices.map(d => `[ID=${d.deviceID}, path=${d.path}]`).join(', '));
    
    // Set up data handler for this device
    newSerial.on('data', (chunk) => onSerialDataMulti(device, chunk));
    newSerial.on('error', e => {
      console.error(`❌ Device ${deviceId} error:`, e.message);
      wsBroadcast({ type: 'serial-error', deviceId, message: e.message });
    });
    newSerial.on('close', () => {
      console.log(`🔌 Device ${deviceId} disconnected: ${desired}`);
      wsBroadcast({ type: 'serial-close', deviceId, path: desired });
      // Remove from devices array
      const index = devices.findIndex(d => d.id === deviceId);
      if (index !== -1) devices.splice(index, 1);
    });
    
    // For first device, also set legacy global variables for backward compatibility
    if (devices.length > 0) {
      serial = newSerial;
      serialPath = desired;
      serialBuf = device.buffer;
      deviceIP = '127.0.0.1';
    }
    
    console.log(`✓ Connected: ${desired} → Device ID ${deviceId} (Tracks ${deviceId * 8 + 1}-${(deviceId + 1) * 8})`);

    // Broadcast device status to UI (wired mode)
    deviceIP = '127.0.0.1';
    wsBroadcast({
      type: 'device-mode',
      mode: 'wired',
      ip: '127.0.0.1'
    });
    
    wsBroadcast({ type: 'serial-open', path: desired, baud: BAUD });
    res.json({ ok: true, path: desired, baud: BAUD, deviceId });
    // Check batch completion after manual connect
    maybeReannounceBatch();
    
    // Wait 3 seconds for device boot before sending init commands
    setTimeout(() => {
      if (devices.find(d => d.id === deviceId && d.path === desired)) {
        console.log(`📤 Sending init commands to Device ${deviceId + 1}...`);
        sendToDevice(deviceId, `DEVICE_ID ${deviceId}\n`);
        setTimeout(() => sendToDevice(deviceId, 'VERSION\n'), 500);
      }
    }, 3000);
  } catch (e) {
    console.error('CRITICAL: /api/connect error:', e);
    uiLog(`FATAL: Connection handler failed: ${e.message}`);
    res.status(500).json({ error: e.message });
  }
  });

// Multi-device: Connect multiple TDS-8 devices
app.post('/api/connect-multi', async (req, res) => {
  try {
    const { ports } = req.body; // Array of COM port paths
    if (!ports || !Array.isArray(ports)) throw new Error('Missing ports array');
    
    const results = [];
    for (let i = 0; i < Math.min(ports.length, MAX_DEVICES); i++) {
      const portPath = ports[i];
      try {
        const deviceId = i; // Auto-assign device ID 0-3
        const newSerial = new SerialPortStream({ binding: Bindings, path: portPath, baudRate: BAUD });
        
        const device = {
          id: deviceId,
          serial: newSerial,
          path: portPath,
          buffer: '',
          version: null,
          deviceID: deviceId
        };
        
        devices.push(device);
        
        // Set up data handler for this device
        newSerial.on('data', (chunk) => onSerialDataMulti(device, chunk));
        
        // Send DEVICE_ID command to set the device's ID
        setTimeout(() => {
          sendToDevice(deviceId, `DEVICE_ID ${deviceId}\n`);
          sendToDevice(deviceId, 'VERSION\n');
        }, 1000);
        
        results.push({ ok: true, deviceId, path: portPath });
        console.log(`✓ Device ${deviceId} connected: ${portPath}`);
      } catch (err) {
        results.push({ ok: false, path: portPath, error: err.message });
      }
    }
    
    wsBroadcast({ type: 'multi-device-connected', devices: results });
    res.json({ ok: true, devices: results });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// Send command to connected device
app.post('/api/send-command', (req, res) => {
  try {
    const { command, deviceId, path } = req.body;
    if (!command) throw new Error('Missing command');
    
    // If deviceId specified, send to that device
    if (deviceId !== undefined && devices.length > 0) {
      console.log(`🔍 [ROUTING] Looking for deviceId=${deviceId} in devices:`, devices.map(d => `[ID=${d.deviceID}, id=${d.id}, path=${d.path}]`).join(', '));
      const device = devices.find(d => d.deviceID === deviceId || d.id === deviceId);
      if (device) {
        sendToDevice(device.id, command + '\n');
        console.log(`📤 [Device ${deviceId}] ${device.path} ← ${command}`);
      } else {
        console.error(`❌ Device ${deviceId} not found in devices array:`, devices.map(d => `ID=${d.deviceID}, path=${d.path}`));
        throw new Error(`Device ${deviceId} not found`);
      }
    } 
    // If path specified, find device by path
    else if (path && devices.length > 0) {
      console.log(`🔍 [ROUTING] Looking for path="${path}" in devices:`, devices.map(d => `[ID=${d.deviceID}, path=${d.path}]`).join(', '));
      const device = devices.find(d => d.path === path);
      if (device) {
        sendToDevice(device.id, command + '\n');
        console.log(`📤 [Device ${device.deviceID}] ${path} ← ${command}`);
      } else {
        console.error(`❌ Device at ${path} not found in devices array:`, devices.map(d => `ID=${d.deviceID}, path=${d.path}`));
        throw new Error(`Device at ${path} not found`);
      }
    }
    // Otherwise send to first device (legacy behavior)
    else {
      send(command + '\n');
      const firstDevice = devices.length > 0 ? devices[0] : null;
      const deviceInfo = firstDevice ? `[Device ${firstDevice.deviceID}] ${firstDevice.path}` : 'first device';
      console.log(`📤 ${deviceInfo} ← ${command}`);
    }
    
    res.json({ ok: true });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/api/disconnect', async (req, res) => {
  try {
    const { path } = req.body;
    
    if (path) {
      // Multi-device: find and disconnect specific device
      const device = devices.find(d => d.path === path);
      if (device) {
        console.log(`🔌 Disconnecting multi-device ${device.id}: ${path}`);
        await new Promise((resolve, reject) => {
          device.serial.close(err => {
            if (err) reject(err);
            else resolve();
          });
        });
        // Device will be removed from devices array by the close event handler
        res.json({ ok: true, message: `Device ${device.id + 1} disconnected` });
      } else {
        res.json({ ok: false, message: `Device at ${path} not found` });
      }
    } else if (serial && serial.isOpen) {
      // Legacy single-device disconnect
      await new Promise((resolve, reject) => {
        serial.close(err => {
          if (err) reject(err);
          else resolve();
        });
      });
      serial = null;
      serialPath = null;
      console.log('🔌 Serial port closed - Available for Arduino IDE');
      wsBroadcast({ type: 'serial-close', path: null });
      res.json({ ok: true, message: 'Serial port released' });
    } else {
      res.json({ ok: true, message: 'No active connection' });
    }
  } catch (e) { 
    console.error('Disconnect error:', e);
    res.status(500).json({ error: e.message }); 
  }
});

// OLD /api/send route - REMOVED (duplicate with multi-device version below)

// Communication toggle - updated for multi-device support
app.post('/api/comm-mode', (req, res) => {
  try {
    const mode = String((req.body && req.body.mode) || '').toLowerCase();
    if (mode !== 'wired') throw new Error('WiFi mode is disabled in this build');
    if (devices.length > 0) {
      console.log(`📢 Enforcing WIRED_ONLY true for ${devices.length} device(s)`);
      sendToAll('WIRED_ONLY true\n');
    } else if (serial && serial.isOpen) {
      send('WIRED_ONLY true\n');
    }
    return res.json({ ok: true, mode: 'wired' });
  } catch (e) {
    console.error('comm-mode error:', e.message);
    return res.status(400).json({ error: e.message });
  }
});

// Wi-Fi join + Windows scan
// WiFi endpoints removed (wired-only build)

// WiFi scan removed (wired-only build)

// GET /tracks - Return current track names for Ableton M4L
// Persist names to disk so they survive server restarts
const TRACK_NAMES_PATH = path.join(__dirname, 'tracknames.json');
let currentTrackNames = [
  'Track 1', 'Track 2', 'Track 3', 'Track 4',
  'Track 5', 'Track 6', 'Track 7', 'Track 8'
];
try {
  const raw = fs.readFileSync(TRACK_NAMES_PATH, 'utf8');
  const parsed = JSON.parse(raw);
  if (Array.isArray(parsed) && parsed.length === 8) {
    currentTrackNames = parsed.map(v => (typeof v === 'string' ? v : String(v || '')));
    console.log('✓ Loaded track names from disk');
  }
} catch (e) {
  // No file yet or parse error; keep defaults silently
}

app.get('/tracks', (_req, res) => {
  res.json({ names: currentTrackNames });
});

// Tracks and active track
app.get('/api/tracknames', (req, res) => {
  try {
    const names = (currentTrackNames || []).map(n => {
      if (typeof n === 'string') return n;
      if (n && typeof n.name === 'string') return n.name;
      return '';
    });
    res.json({ ok: true, names });
  } catch (e) { res.status(400).json({ error: e.message }); }
});

// New endpoint to save all track names to a file
app.post('/api/tracknames', (req, res) => {
    const { names } = req.body;
    if (!Array.isArray(names)) {
        return res.status(400).json({ ok: false, error: 'Invalid data format' });
    }

    const filePath = path.join(__dirname, 'track_names.json');
    const data = JSON.stringify({ names }, null, 2);

    fs.writeFile(filePath, data, 'utf8', (err) => {
        if (err) {
            console.error('❌ Error saving track names:', err);
            return res.status(500).json({ ok: false, error: 'Failed to save track names' });
        }
        return res.json({ ok: true });
    });
});

app.post('/api/trackname', (req, res) => {
  try {
    const { index, name, actualTrack } = req.body || {};

    // Debug: log exactly what we received
    console.log(' [DEBUG] Received trackname request:', {
      index,
      name,
      nameType: typeof name,
      nameLength: name ? name.length : 0,
      actualTrack
    });

    // Allow empty string name to clear display; only require the field to be present
    if (name === undefined) throw new Error('Missing name');
    if (index >= 0 && index < 8) currentTrackNames[index] = String(name);

    // Build safe, quoted name and always include actualTrack (device adds +1, so default to index)
    let nameStr = String(name);
    nameStr = nameStr.trim();
    // If the entire name is wrapped in quotes, strip the outer quotes to avoid double-quoting on serial
    if (nameStr.length >= 2 && nameStr.startsWith('"') && nameStr.endsWith('"')) {
      nameStr = nameStr.slice(1, -1);
    }
    const esc = nameStr.replace(/"/g, '\\"');
    const escNorm = normalizeName(name);
    const at = (actualTrack !== undefined) ? actualTrack : Number(index);
    const cmd = `/trackname ${index} "${esc}" ${at}\n`;
    // Global dedup: suppress duplicate track name updates for the same actual track
    if (!shouldForwardGlobal(Number(at), escNorm)) {
      console.log(` Deduped global /api/trackname for track ${at}`);
      return res.json({ ok: true, dedup: true });
    }

    // Calculate which device should receive this track (based on actualTrack)
    const targetDeviceId = Math.floor(at / 8);

    console.log('[TRACKNAME] Routing:', {
      index,
      name: name,
      actualTrack: at,
      targetDeviceId,
      devicesCount: devices.length,
      availableDevices: devices.map(d => `ID=${d.deviceID}`).join(', ')
    });

    // Route to correct device if multi-device mode
    if (devices.length > 1) {
      console.log(` [ROUTING] Looking for deviceId=${targetDeviceId} for track ${at}`);
      const targetDevice = devices.find(d => d.deviceID === targetDeviceId || d.id === targetDeviceId);
      if (targetDevice) {
        if (shouldSendTrackname(targetDevice.id, index, escNorm)) {
          sendToDevice(targetDevice.id, cmd);
          console.log(` [Device ${targetDeviceId}] ${targetDevice.path} ← Track ${at}: "${name}"`);
        } else {
          console.log(` Deduped /api/trackname for Device ${targetDevice.id} idx ${index}`);
        }
      } else {
        console.warn(` Device ${targetDeviceId} not found for track ${at}. Available:`, devices.map(d => `[ID=${d.deviceID}, path=${d.path}]`).join(', '));
        console.warn(` Falling back to first device`);
        if (shouldSendTrackname(0, index, escNorm)) send(cmd);
      }
    } else {
      // Single device mode: prefer multi-device pipe if available
      if (devices.length > 0) {
        const d0 = devices[0];
        if (shouldSendTrackname(d0.id, index, escNorm)) {
          sendToDevice(d0.id, cmd);
          console.log(` [Device ${d0.deviceID}] ${d0.path} ← Track ${at}: "${name}"`);
        } else {
          console.log(` Deduped /api/trackname for Device ${d0.id} idx ${index}`);
        }
      } else if (serial && serial.isOpen) {
        // Legacy single-serial fallback
        if (shouldSendTrackname(0, index, escNorm)) send(cmd);
        console.log(` [Serial] ← Track ${at}: "${name}"`);
      } else {
        throw new Error('No connected device to send trackname');
      }
    }

    // Persist asynchronously; ignore errors
    try { fs.promises.writeFile(TRACK_NAMES_PATH, JSON.stringify(currentTrackNames, null, 2), 'utf8'); } catch {}
    res.json({ ok: true });
  } catch (e) { 
    console.error(' /api/trackname error:', e);
    res.status(400).json({ error: e.message }); 
  }
});

app.post('/api/activetrack', (req, res) => {
  try {
    const { index } = req.body || {};
    if (!Number.isFinite(index)) throw new Error('Missing index');

    if (devices.length > 0) {
      const deviceId = Math.floor(index / 8);
      const localIndex = index % 8;
      const targetDevice = devices.find(d => d.id === deviceId);
      if (targetDevice) {
        console.log(`📢 Routing ACTIVETRACK ${index} → Device ${deviceId} (local ${localIndex})`);
        // Send selection to target device
        sendToDevice(deviceId, `/activetrack ${localIndex}\n`);
        // Clear selection on all other devices
        devices.filter(d => d.id !== deviceId && d.serial && d.serial.isOpen)
               .forEach(d => sendToDevice(d.id, `/activetrack -1\n`));
      } else if (devices.length === 1) {
        console.log(`📢 Single device present; sending local index ${localIndex}`);
        sendToDevice(devices[0].id, `/activetrack ${localIndex}\n`);
      } else {
        console.log(`📢 No exact device match; clearing all`);
        sendToAll(`/activetrack -1\n`);
      }
    } else {
      send(`/activetrack ${index}\n`);
    }

    res.json({ ok: true });
  } catch (e) {
    console.error('❌ /api/activetrack error:', e);
    res.status(400).json({ error: e.message });
  }
});

app.post('/api/send', (req, res) => {
    try {
        const { cmd } = req.body;
        if (!cmd) throw new Error('Missing cmd');

        const upperCmd = cmd.toUpperCase().trim();
        
        // Handle VERSION commands
        if (upperCmd.startsWith('VERSION')) {
            const parts = upperCmd.split(/\s+/);
            
            // VERSION or VERSION ALL → query all devices
            if (parts.length === 1 || (parts.length === 2 && parts[1] === 'ALL')) {
                if (devices.length === 0) {
                    throw new Error('No devices connected');
                }
                console.log(`📢 Broadcasting VERSION to all ${devices.length} devices`);
                sendToAll('VERSION\n');
                return res.json({ ok: true, message: `VERSION sent to all ${devices.length} devices` });
            }
            
            // VERSION <number> → query specific device
            if (parts.length === 2) {
                const deviceNum = parseInt(parts[1]);
                if (isNaN(deviceNum)) {
                    throw new Error(`Invalid device number: ${parts[1]}`);
                }
                
                // Device numbers are 1-indexed for user (Device 1, Device 2, etc.)
                // But device IDs are 0-indexed internally (ID 0, ID 1, etc.)
                const deviceId = deviceNum - 1;
                
                const targetDevice = devices.find(d => d.id === deviceId);
                if (!targetDevice) {
                    throw new Error(`Device ${deviceNum} not found or not connected`);
                }
                
                console.log(`📋 Requesting VERSION from Device ${deviceNum} (ID: ${deviceId})`);
                sendToDevice(deviceId, 'VERSION\n');
                return res.json({ ok: true, message: `VERSION request sent to Device ${deviceNum}` });
            }
            
            throw new Error('Invalid VERSION syntax. Use: VERSION, VERSION ALL, or VERSION <number>');
        }
        
        // Handle WIRED_ONLY <true|false> (always broadcast)
        if (upperCmd.startsWith('WIRED_ONLY')) {
            const parts = cmd.trim().split(/\s+/);
            const arg = parts.length >= 2 ? parts[1] : undefined;
            if (!/^(true|false)$/i.test(String(arg || ''))) {
                throw new Error('Invalid argument. Use: WIRED_ONLY true|false');
            }
            if (/^false$/i.test(arg)) {
                throw new Error('WiFi mode is disabled in this build');
            }
            if (devices.length === 0) throw new Error('No devices connected');
            console.log(`📢 Enforcing WIRED_ONLY true for all ${devices.length} devices`);
            sendToAll('WIRED_ONLY true\n');
            return res.json({ ok: true, message: 'WIRED_ONLY true sent to all devices' });
        }

        // Handle /reannounce command (send OSC to M4L)
        if (upperCmd === '/REANNOUNCE') {
            console.log('📡 Sending /reannounce OSC to M4L');
            try {
                sendOSC('/reannounce', []);
                return res.json({ ok: true, message: '/reannounce sent to M4L via OSC' });
            } catch (err) {
                return res.status(400).json({ error: `Failed to send /reannounce: ${err.message}` });
            }
        }
        
        // Handle REBOOT commands
        if (upperCmd.startsWith('REBOOT')) {
            const parts = upperCmd.split(/\s+/);
            
            // REBOOT or REBOOT ALL → reboot all devices
            if (parts.length === 1 || (parts.length === 2 && parts[1] === 'ALL')) {
                if (devices.length === 0) {
                    throw new Error('No devices connected to reboot');
                }
                console.log(`📢 Rebooting all ${devices.length} devices`);
                sendToAll('REBOOT\n');
                return res.json({ ok: true, message: `Reboot command sent to all ${devices.length} devices` });
            }
            
            // REBOOT <number> → reboot specific device
            if (parts.length === 2) {
                const deviceNum = parseInt(parts[1]);
                if (isNaN(deviceNum)) {
                    throw new Error(`Invalid device number: ${parts[1]}`);
                }
                
                // Device numbers are 1-indexed for user (Device 1, Device 2, etc.)
                // But device IDs are 0-indexed internally (ID 0, ID 1, etc.)
                const deviceId = deviceNum - 1;
                
                console.log(`🔍 [DEBUG] Looking for Device ${deviceNum} (deviceId=${deviceId})`);
                console.log(`🔍 [DEBUG] Available devices:`, devices.map(d => `[id=${d.id}, path=${d.path}]`).join(', '));
                
                const targetDevice = devices.find(d => d.id === deviceId);
                if (!targetDevice) {
                    throw new Error(`Device ${deviceNum} not found or not connected. Available devices: ${devices.map(d => `Device ${d.id + 1}`).join(', ')}`);
                }
                
                console.log(`🔄 Rebooting Device ${deviceNum} (ID: ${deviceId}) on path: ${targetDevice.path}`);
                sendToDevice(deviceId, 'REBOOT\n');
                return res.json({ ok: true, message: `Reboot command sent to Device ${deviceNum}` });
            }
            
            throw new Error('Invalid REBOOT syntax. Use: REBOOT, REBOOT ALL, or REBOOT <number>');
        }
        
        // Other commands: send to first device as default (explicit broadcast not defined)
        if (devices.length > 0) {
            sendToDevice(devices[0].id, cmd + '\n');
            res.json({ ok: true, message: `Command sent to device ${devices[0].id}.` });
        } else {
            throw new Error('No devices connected to send command.');
        }
    } catch (e) {
        res.status(400).json({ error: e.message });
    }
});

// Send arbitrary OSC message to M4L
app.post('/api/osc-send', (req, res) => {
  try {
    const { address, args } = req.body || {};
    if (!address) throw new Error('Missing OSC address');
    // Debounce /reannounce triggered from UI/API
    if (address === '/reannounce') {
      requestReannounce();
      return res.json({ ok: true, message: 'Debounced /reannounce' });
    }
    
    const port = initOSC();
    if (!port) throw new Error('OSC not initialized');
    
    // Build OSC message
    const oscMsg = {
      address: address,
      args: args && Array.isArray(args) ? args.map(arg => ({
        type: typeof arg === 'number' ? (Number.isInteger(arg) ? 'i' : 'f') : 's',
        value: arg
      })) : []
    };
    
    port.send(oscMsg, M4L_IP, M4L_PORT);
    const logMessage = `📡 OSC sent to M4L: ${address} ${args ? args.join(' ') : ''}`;
    console.log(logMessage);
    wsBroadcast({ type: 'log', level: 'info', message: logMessage });
    
    res.json({ ok: true, message: `Sent ${address} to M4L` });
  } catch (e) { 
    console.error('OSC send error:', e.message);
    res.status(400).json({ ok: false, error: e.message }); 
  }
});

// Broadcast IP endpoint removed (wired-only build)

// Get current device status
app.get('/api/device-status', (req, res) => {
  res.json({ 
    ok: true,
    ip: '127.0.0.1',
    connectionType: 'wired'
  });
});

// Diagnostic endpoint - check OSC port status
app.get('/api/osc-status', (req, res) => {
  const now = Date.now();
  const timeSinceLastHi = lastHiTime ? now - lastHiTime : null;
  const abletonConnected = timeSinceLastHi !== null && timeSinceLastHi < 30000;
  
  const socketReady = udpPort && udpPort.socket && udpPort.socket._handle ? true : false;
  
  res.json({
    ok: true,
    oscSenderPort: 9001,
    oscListenerPort: OSC_LISTEN_PORT,
    deviceListenerPort: 9000,
    m4lTargetPort: M4L_PORT,
    m4lTargetIP: M4L_IP,
    lastHiReceived: lastHiTime ? new Date(lastHiTime).toISOString() : 'Never',
    timeSinceLastHi: timeSinceLastHi ? `${Math.floor(timeSinceLastHi / 1000)}s ago` : 'Never',
    abletonConnected: abletonConnected,
    udpPortInitialized: udpPort !== null,
    udpSocketReady: socketReady,
    oscListenerInitialized: oscListener !== null,
    connectedDevices: devices.length,
    deviceList: devices.map(d => ({ id: d.deviceID, path: d.path, version: d.version }))
  });
  
  console.log('🔍 [OSC STATUS] Diagnostic check:', {
    abletonConnected,
    lastHi: lastHiTime ? `${Math.floor(timeSinceLastHi / 1000)}s ago` : 'Never',
    oscSenderReady: socketReady,
    oscListenerReady: oscListener !== null,
    connectedDevices: devices.length
  });
});

// Test endpoint - manually send /hello to M4L
app.post('/api/osc-test', (req, res) => {
  console.log('🧪 [TEST] Manually sending /hello to M4L...');
  try {
    sendOSC('/hello', []);
    res.json({ ok: true, message: 'Sent /hello to M4L - check console for response' });
  } catch (e) {
    res.status(500).json({ ok: false, error: e.message });
  }
});

// Firmware — check single manifest
app.get('/api/ota-check', async (req, res) => {
  try {
    const manifestUrl = req.query.manifest || OTA_MANIFEST_URL;
    
    // Try local manifest first if no URL or GitHub URL
    if (!manifestUrl || manifestUrl.includes('github.com')) {
      try {
        const localManifest = JSON.parse(await fs.promises.readFile(path.join(__dirname, 'manifest.json'), 'utf8'));
        const newer = deviceVersion ? compareVersions(localManifest.version, deviceVersion) > 0 : null;
        return res.json({ ok: true, manifest: localManifest, deviceVersion, isNewer: newer, source: 'local' });
      } catch (localErr) {
        console.log('Local manifest not found:', localErr.message);
        if (!manifestUrl) {
          return res.json({ ok: false, error: 'No manifest URL configured and no local manifest found', deviceVersion });
        }
        // Continue to try remote URL
      }
    }
    
    // Try fetching from remote URL
    const mPath = await fetchToTemp(manifestUrl);
    const manifest = JSON.parse(await fs.promises.readFile(mPath, 'utf8'));
    const newer = deviceVersion ? compareVersions(manifest.version, deviceVersion) > 0 : null;
    res.json({ ok: true, manifest, deviceVersion, isNewer: newer, source: 'remote' });
  } catch (e) { res.status(400).json({ error: e.message }); }
});

// Firmware — load a feed of versions (optional)

// New endpoint to switch all devices to WiFi mode
// WiFi switch endpoint removed (wired-only build)

// Firmware — start OTA from a manifest - updated for multi-device support
app.post('/api/ota-update', async (req, res) => {
  // Disabled in wired-only build
  return res.status(501).json({ ok: false, error: 'WiFi/OTA is disabled in this build' });
});

// Flash firmware over USB using esptool (wired-only)
let FLASH_IN_PROGRESS = false;

app.post('/api/flash', async (req, res) => {
  try {
    if (FLASH_IN_PROGRESS) {
      return res.status(429).json({ ok: false, error: 'Another flash is in progress. Please wait.' });
    }
    const { url, manifest, deviceId, path: portPathOverride } = req.body || {};
    let fwUrl = url;
    if (!fwUrl && manifest) {
      try {
        const mPath = await fetchToTemp(manifest);
        const man = JSON.parse(await fs.promises.readFile(mPath, 'utf8'));
        fwUrl = man && man.url ? man.url : null;
      } catch (e) {
        throw new Error('Failed to read manifest: ' + e.message);
      }
    }
    if (!fwUrl) throw new Error('Missing firmware url or manifest');

    // Decide which port to flash
    let targetDevice = null;
    if (typeof deviceId === 'number') {
      targetDevice = devices.find(d => d.id === deviceId || d.deviceID === deviceId) || null;
    }
    let portPath = portPathOverride || (targetDevice ? targetDevice.path : (devices[0] && devices[0].path) || serialPath);
    if (!portPath) throw new Error('No connected device to flash');

    // Download firmware to temp
    const fwPath = await fetchToTemp(fwUrl);

    // Close serial if open on this port
    try {
      if (targetDevice && targetDevice.serial && targetDevice.serial.isOpen) {
        await new Promise((resolve, reject) => targetDevice.serial.close(err => err ? reject(err) : resolve()));
      } else if (serial && serial.isOpen && serialPath === portPath) {
        await new Promise((resolve, reject) => serial.close(err => err ? reject(err) : resolve()));
      }
    } catch {}

    // Try python/esptool runners in order (streamed, no maxBuffer)
    const localEsptool = process.platform === 'win32' ? path.join(__dirname, 'bin', 'esptool.exe') : path.join(__dirname, 'bin', 'esptool');
    const runners = [
      { cmd: localEsptool, args: ['--chip', 'esp32c3', '--port', portPath, '--baud', '921600', 'write_flash', '-z', '0x10000', fwPath] },
      { cmd: 'python', args: ['-m', 'esptool', '--chip', 'esp32c3', '--port', portPath, '--baud', '921600', 'write_flash', '-z', '0x10000', fwPath] },
      { cmd: 'py', args: ['-m', 'esptool', '--chip', 'esp32c3', '--port', portPath, '--baud', '921600', 'write_flash', '-z', '0x10000', fwPath] },
      { cmd: 'python3', args: ['-m', 'esptool', '--chip', 'esp32c3', '--port', portPath, '--baud', '921600', 'write_flash', '-z', '0x10000', fwPath] },
      { cmd: 'esptool.py', args: ['--chip', 'esp32c3', '--port', portPath, '--baud', '921600', 'write_flash', '-z', '0x10000', fwPath] }
    ];

    FLASH_IN_PROGRESS = true;
    let lastErr = null;
    for (const r of runners) {
      try {
        uiLog(`[FLASH] Running: ${r.cmd} ${r.args.join(' ')}`);
        await new Promise((resolve, reject) => {
          const child = spawn(r.cmd, r.args, { windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] });
          let done = false;
          const finish = (err) => { if (!done) { done = true; err ? reject(err) : resolve(); } };
          child.stdout.on('data', d => { const t = d.toString(); if (t.trim()) uiLog(t); });
          child.stderr.on('data', d => { const t = d.toString(); if (t.trim()) uiLog(t); });
          child.on('error', e => finish(e));
          child.on('close', code => {
            if (code === 0) finish(); else finish(new Error(`esptool exit ${code}`));
          });
        });
        uiLog('[FLASH] Completed successfully. Device will reboot.');
        FLASH_IN_PROGRESS = false;
        return res.json({ ok: true });
      } catch (e) {
        lastErr = e;
        uiLog(`[FLASH] Failed with runner: ${e.message}`);
        continue;
      }
    }
    FLASH_IN_PROGRESS = false;
    throw new Error(lastErr ? lastErr.message : 'Unknown flashing error');
  } catch (e) {
    console.error('Flash error:', e);
    return res.status(400).json({ ok: false, error: e.message });
  }
});

// -------- Utils --------
async function fetchToTemp(url, redirects = 5) {
  const isHttps = url.startsWith('https:');
  const mod = await (isHttps ? import('node:https') : import('node:http'));
  return await new Promise((resolve, reject) => {
    const req = mod.request(url, resp => {
      // Follow redirects (e.g., GitHub release asset -> S3)
      if (resp.statusCode && resp.statusCode >= 300 && resp.statusCode < 400 && resp.headers && resp.headers.location) {
        if (redirects <= 0) { reject(new Error('Too many redirects')); return; }
        const nextUrl = new URL(resp.headers.location, url).toString();
        resp.resume(); // discard
        fetchToTemp(nextUrl, redirects - 1).then(resolve).catch(reject);
        return;
      }
      if (!resp.statusCode || resp.statusCode < 200 || resp.statusCode >= 300) { reject(new Error('HTTP ' + (resp.statusCode || 'ERR'))); return; }
      const tmp = path.join(require('os').tmpdir(), 'tds8-' + Date.now() + '-' + Math.random().toString(16).slice(2));
      const f = fs.createWriteStream(tmp);
      resp.pipe(f);
      f.on('finish', () => f.close(() => resolve(tmp)));
    });
    req.on('error', reject);
    req.end();
  });
}

function compareVersions(a, b) {
  const pa = String(a).split('.').map(n => parseInt(n, 10));
  const pb = String(b).split('.').map(n => parseInt(n, 10));
  const len = Math.max(pa.length, pb.length);
  for (let i = 0; i < len; i++) { const da = pa[i] || 0, db = pb[i] || 0; if (da > db) return 1; if (da < db) return -1; }
  return 0;
}

wss.on('connection', ws => {
  ws.send(JSON.stringify({ type: 'hello', port: serialPath, baud: BAUD }));
  try {
    const msg1 = `OSC: Bridge listening ${OSC_LISTEN_PORT} (from M4L), sending ${M4L_PORT} (to M4L)`;
    const msg3 = `OSC Sender: 127.0.0.1:9001 -> ${M4L_IP}:${M4L_PORT}`;
    ws.send(JSON.stringify({ type: 'serial-data', data: msg1 }));
    ws.send(JSON.stringify({ type: 'serial-data', data: msg3 }));
  } catch {}
});

// Auto-connect to all TDS-8 devices
async function autoConnectDevices() {
  try {
    const now = Date.now();
    const ports = await listPorts();
    let tds8Ports = ports.filter(p => {
      const vid = (p.vendorId || '').toUpperCase();
      const pid = (p.productId || '').toUpperCase();
      const manu = (p.manufacturer || '').toLowerCase();
      const label = (p.pnpId || '').toLowerCase();
      // Match ESP32-C3 default, Arduino/Seeed labels, and common CH340 chips
      const isEsp32C3 = (vid === '303A' && pid === '1001');
      const isArduino = manu.includes('arduino') || label.includes('arduino');
      const isSeeed = manu.includes('seeed') || label.includes('seeed');
      const isCH340 = (vid === '1A86');
      const isCP210x = (vid === '10C4') || manu.includes('silicon labs') || label.includes('cp210');
      return isEsp32C3 || isArduino || isSeeed || isCH340 || isCP210x;
    });
    // Fallback: if no matches, try any COM* ports on Windows
    if (tds8Ports.length === 0 && process.platform === 'win32') {
      try {
        await new Promise((resolve) => {
          const cmd = 'powershell -NoProfile -Command "try { Get-CimInstance Win32_SerialPort | Select-Object -ExpandProperty DeviceID } catch { try { Get-WmiObject Win32_SerialPort | Select-Object -ExpandProperty DeviceID } catch {} }"';
          exec(cmd, { windowsHide: true, timeout: 4000 }, (err, stdout) => {
            if (!err && stdout) {
              const lines = stdout.split(/\r?\n/).map(s => s.trim()).filter(Boolean);
              const extra = lines.filter(s => /^COM\d+$/i.test(s)).map(id => ({ path: id }));
              if (extra.length > 0) {
                console.log('➕ AutoConnect: discovered COM ports via PowerShell:', extra.map(e => e.path));
                // Merge any not already in listPorts()
                const known = new Set(ports.map(p => p.path));
                tds8Ports = extra.filter(e => !known.has(e.path)).concat(ports);
              }
            }
            resolve();
          });
        });
      } catch {}
      // Final fallback: use all ports if still nothing
      if (tds8Ports.length === 0) tds8Ports = ports;
    }
    
    console.log(`🔍 Found ${tds8Ports.length} potential TDS-8 device(s)`);
    // For this scan, expect this many devices; we'll fire one /reannounce when all are connected
    expectedDeviceCount = Math.min(tds8Ports.length, MAX_DEVICES);
    batchReannounceDone = false;
    
    for (const port of tds8Ports) {
      // Check if already connected
      const alreadyConnected = devices.find(d => d.path === port.path);
      // Respect cooldown if this port was just closed (common during reboot)
      const lastClosed = recentlyClosed.get(port.path) || 0;
      const coolingDown = (now - lastClosed) < RECONNECT_COOLDOWN_MS;
      if (coolingDown) {
        console.log(`⏳ Skipping ${port.path} (cooldown ${(RECONNECT_COOLDOWN_MS - (now - lastClosed))}ms)`);
        continue;
      }
      if (alreadyConnected || connectingPorts.has(port.path)) {
        if (alreadyConnected) console.log(`⏭️  ${port.path} already connected as Device ${alreadyConnected.id}`);
        if (connectingPorts.has(port.path)) console.log(`⏳ ${port.path} connection in progress...`);
        continue;
      }
      
      // Determine device ID: check port mapping first, then assign next available
      let deviceId;
      if (portToDeviceId.has(port.path)) {
        // This port has been used before - restore its ID
        deviceId = portToDeviceId.get(port.path);
        console.log(`📌 Auto-connect restoring Device ID ${deviceId} for ${port.path} (previously assigned)`);
      } else {
        // New port - assign next available ID (0-3)
        const usedIds = new Set(Array.from(portToDeviceId.values()));
        for (let i = 0; i < MAX_DEVICES; i++) {
          if (!usedIds.has(i)) {
            deviceId = i;
            break;
          }
        }
        portToDeviceId.set(port.path, deviceId);
        console.log(`🆕 Auto-connect assigning new Device ID ${deviceId} to ${port.path}`);
      }
      
      if (deviceId === undefined) {
        console.log(`🚫 No available Device ID for ${port.path}, skipping auto-connect`);
        continue;
      }

      console.log(`🔌 Auto-connecting ${port.path} as Device ${deviceId + 1}...`);
      connectingPorts.add(port.path); // Add to connecting set
      
      try {
        const newSerial = new SerialPortStream({ 
          binding: Bindings, 
          path: port.path, 
          baudRate: BAUD,
          autoOpen: false
        });
        
        const device = {
          id: deviceId,
          serial: newSerial,
          path: port.path,
          buffer: '',
          version: null,
          deviceID: deviceId
        };
        
        // Open port and wait for stabilization
        await newSerial.open();
        await new Promise(res => setTimeout(res, 1000));
        
        devices.push(device);
        
        newSerial.on('data', (chunk) => onSerialDataMulti(device, chunk));
        newSerial.on('error', e => {
          console.error(`❌ Device ${deviceId} error:`, e.message);
        });
        newSerial.on('close', () => {
          console.log(`🔌 Device ${deviceId} disconnected: ${port.path}`);
          const index = devices.findIndex(d => d.id === deviceId);
          if (index !== -1) devices.splice(index, 1);
          // Do NOT auto-reconnect - let user manually reconnect like Arduino IDE
          recentlyClosed.set(port.path, Date.now());
          connectingPorts.delete(port.path);
          wsBroadcast({ type: 'device-disconnected', deviceId, path: port.path });
          // Allow a new batch reannounce next time
          batchReannounceDone = false;
        });
        
        console.log(`✅ Auto-connected: ${port.path} → Device ${deviceId + 1} (Tracks ${deviceId * 8 + 1}-${(deviceId + 1) * 8})`);
        wsBroadcast({ type: 'device-connected', deviceId, path: port.path, trackRange: `${deviceId * 8 + 1}-${(deviceId + 1) * 8}` });
        // Check if we have reached expected device count and announce once
        maybeReannounceBatch();
        
        // Wait 3 seconds for device boot before sending init commands
        setTimeout(() => {
          if (devices.find(d => d.id === deviceId && d.path === port.path)) {
            console.log(`📤 Sending init commands to Device ${deviceId + 1}...`);
            sendToDevice(deviceId, `DEVICE_ID ${deviceId}\n`);
            setTimeout(() => sendToDevice(deviceId, 'VERSION\n'), 500);
          }
        }, 3000);
        
      } catch (err) {
        console.error(`❌ Failed to auto-connect ${port.path}:`, err.message);
        connectingPorts.delete(port.path); // Remove from connecting set on error
      }
    }
  } catch (err) {
    console.error('❌ Auto-connect failed:', err.message);
  }
}

server.listen(PORT, () => {
  console.log('\n========================================================');
  console.log('         TDS-8 Bridge - Port Configuration');
  console.log('========================================================');
  console.log(`  HTTP Server:    http://localhost:${PORT}`);
  console.log(`  OSC Listener:   0.0.0.0:${OSC_LISTEN_PORT} (receives from M4L)`);
  console.log(`  OSC Sender:     127.0.0.1:9001 -> 127.0.0.1:${M4L_PORT}`);
  console.log('  WebSocket:      ws://localhost:8088');
  console.log('========================================================');
  console.log(`  M4L should send OSC to: 127.0.0.1:${OSC_LISTEN_PORT}`);
  console.log(`  M4L should listen on:   port ${M4L_PORT}`);
  console.log('========================================================\n');
  
  // Auto-connect scheduling (with safety delays)
  // First scan shortly after startup, then periodic rescans
  setTimeout(() => {
    try {
      console.log('🔍 Starting auto-connect scan...');
      autoConnectDevices();
    } catch (e) {
      console.error('Auto-connect initial scan failed:', e.message);
    }
  }, 3000);
  setInterval(() => {
    try {
      autoConnectDevices();
    } catch (e) {
      console.error('Auto-connect interval failed:', e.message);
    }
  }, 10000);
});
