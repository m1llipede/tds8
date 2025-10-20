#!/usr/bin/env node
// TDS-8 Desktop Bridge — comm toggle (wired|wifi), Wi-Fi scan (Windows), batch tracks, firmware feed + manifest

const path = require('path');
const fs = require('fs');
const express = require('express');
const morgan = require('morgan');
const http = require('http');
const WebSocket = require('ws');
const osc = require('osc');
const { exec } = require('child_process');
const { SerialPortStream } = require('@serialport/stream');
const { autoDetect } = require('@serialport/bindings-cpp');

const PORT = process.env.PORT || 8088;
const OSC_LISTEN_PORT = parseInt(process.env.OSC_LISTEN_PORT || '8000', 10);
const BAUD = parseInt(process.env.BAUD || '115200', 10);

// Optional defaults (you can set these later)
const OTA_MANIFEST_URL = process.env.OTA_MANIFEST_URL || '';
const FW_FEED_URL = process.env.FW_FEED_URL || '';

const Bindings = autoDetect();

// Multi-device support: Store up to 4 TDS-8 devices
const MAX_DEVICES = 4;
let devices = []; // Array of { id, serial, path, buffer, version, deviceID }
let deviceVersion = null; // Legacy - for backward compatibility

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// OSC UDP Port for broadcasting to Ableton M4L (lazy init)
let udpPort = null;

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
                console.log("📡 OSC Sender: Ready on port 9001 (sends to Ableton on 127.0.0.1:9000)");
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

// Track device IP for WiFi mode
let deviceIP = "127.0.0.1"; // Default to localhost for wired mode

// Send OSC message to Ableton
function sendOSC(address, args = []) {
    const port = initOSC();
    if (port) {
        try {
            port.send({
                address: address,
                args: args
            }, deviceIP, 9000); // Use tracked device IP (localhost or WiFi IP)
            const argsStr = args.length > 0 ? ' [' + args.map(a => a.value || a).join(', ') + ']' : '';
            console.log(`Sent to Ableton (${deviceIP}): ${address}${argsStr}`);
            
            // Broadcast to browser UI
            wsBroadcast({ 
                type: 'osc-sent', 
                address: address, 
                args: args,
                argsStr: argsStr
            });
        } catch (err) {
            console.error(`OSC send error (${address}):`, err.message);
        }
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
            let abletonConnected = false;
            let lastHelloResponse = 0;
            
            // Function to check connection and send hello
            const checkConnection = () => {
                // Send /hello
                console.log('>>> Handshake check...');
                sendOSC('/hello', []);
                
                // Check if we got /hi in last 30 seconds
                const now = Date.now();
                const wasConnected = abletonConnected;
                abletonConnected = (now - lastHelloResponse) < 30000;
                
                if (wasConnected !== abletonConnected) {
                    console.log(`${abletonConnected ? '✓ Ableton CONNECTED' : '✗ Ableton DISCONNECTED'}`);
                    try {
                        if (serial && serial.isOpen) {
                            const cmd = abletonConnected ? '/ableton_on\n' : '/ableton_off\n';
                            send(cmd);
                        }
                    } catch (err) {
                        console.error('Error sending Ableton status:', err);
                    }
                }
            };
            
            // Send first hello immediately after 2 seconds
            setTimeout(() => {
                console.log('🔔 Starting Ableton handshake...');
                checkConnection();
            }, 2000);
            
            // Then send /hello every 10 seconds
            setInterval(checkConnection, 10000);
            
            oscListener.on("message", (oscMsg, timeTag, info) => {
                const fromIP = info.address || 'unknown';
                const fromPort = info.port || 'unknown';
                
                // Format args for display
                const argsStr = oscMsg.args && oscMsg.args.length > 0 
                    ? oscMsg.args.map(a => a.value).join(', ') 
                    : '';
                console.log(`Received: ${oscMsg.address}${argsStr ? ' [' + argsStr + ']' : ''}`);
                
                wsBroadcast({ type: 'osc-received', from: `${fromIP}:${fromPort}`, address: oscMsg.address, args: oscMsg.args });
                
                // Handle /ipupdate from device (when it connects to WiFi)
                if (oscMsg.address === "/ipupdate") {
                    if (oscMsg.args.length > 0) {
                        const newIP = oscMsg.args[0].value;
                        const ssid = oscMsg.args.length > 1 ? oscMsg.args[1].value : null;
                        
                        console.log(`RECV: /ipupdate ${newIP}`);
                        
                        if (newIP && newIP !== deviceIP) {
                            deviceIP = newIP;
                            
                            wsBroadcast({ 
                                type: 'device-ip', 
                                ip: deviceIP,
                                ssid: ssid,
                                connectionType: newIP === '127.0.0.1' ? 'wired' : 'wifi'
                            });
                            
                            // Forward to M4L
                            broadcastIPUpdate();
                        }
                    }
                    return;
                }
                
                // Handle /hi response to our /hello
                if (oscMsg.address === "/hi") {
                    lastHelloResponse = Date.now();
                    const wasConnected = abletonConnected;
                    abletonConnected = true;
                    
                    // Send /ableton_on EVERY time we get /hi (triggers heartbeat flash)
                    try {
                        if (serial && serial.isOpen) send('/ableton_on\n');
                    } catch (err) {}
                    
                    if (!wasConnected) {
                        console.log('✓ Ableton CONNECTED');
                        
                        // Send /reannounce to M4L via OSC to request track names
                        setTimeout(() => {
                            try {
                                sendOSC('/reannounce', []);
                            } catch (err) {
                                console.error('Failed to send /reannounce:', err.message);
                            }
                        }, 1000);
                    }
                    return;
                }
                
                // Forward OSC commands to device via serial
                if (oscMsg.address === "/trackname" && oscMsg.args.length >= 2) {
                    const displayIndex = oscMsg.args[0].value;
                    const nameRaw = oscMsg.args[1].value;
                    const actualTrack = oscMsg.args.length >= 3 ? oscMsg.args[2].value : displayIndex;

                    const esc = String(nameRaw).replace(/"/g, '\\"');
                    const cmd = `/trackname ${displayIndex} "${esc}" ${actualTrack}\n`;
                    send(cmd);

                    // Store the UNESCAPED name for UI/API (not the escaped version)
                    if (displayIndex >= 0 && displayIndex < 8) {
                        currentTrackNames[displayIndex] = String(nameRaw);
                    }
                }
                else if (oscMsg.address === "/activetrack" && oscMsg.args.length >= 1) {
                    const index = oscMsg.args[0].value;
                    send(`/activetrack ${index}\n`);
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

// OSC Listener on port 9000 for receiving /ipupdate from TDS-8 device
let deviceListener = null;

function initDeviceListener() {
    if (!deviceListener) {
        try {
            deviceListener = new osc.UDPPort({
                localAddress: "0.0.0.0",
                localPort: 9000,
                metadata: true
            });
            
            deviceListener.on("message", (oscMsg) => {
                // Only handle /ipupdate from device
                if (oscMsg.address === "/ipupdate" && oscMsg.args.length > 0) {
                    const newIP = oscMsg.args[0].value;
                    const ssid = oscMsg.args.length > 1 ? oscMsg.args[1].value : null;
                    
                    console.log(`RECV: /ipupdate ${newIP} (from device)`);
                    
                    if (newIP) {
                        const changed = newIP !== deviceIP;
                        deviceIP = newIP;
                        
                        const connectionType = newIP === '127.0.0.1' ? 'wired' : 'wifi';
                        console.log(`📡 Broadcasting device-ip to UI: ${connectionType} @ ${deviceIP}`);
                        
                        // Always broadcast to UI (even if IP didn't change, UI might need update)
                        wsBroadcast({ 
                            type: 'device-ip', 
                            ip: deviceIP,
                            ssid: ssid,
                            connectionType: connectionType
                        });
                        
                        // Forward to M4L only if IP changed
                        if (changed) {
                            broadcastIPUpdate();
                            
                            // Send /reannounce to M4L to request track names
                            setTimeout(() => {
                                try {
                                    sendOSC('/reannounce', []);
                                    console.log('SENT: /reannounce (after WiFi connection)');
                                } catch (err) {
                                    console.error('Failed to send /reannounce:', err.message);
                                }
                            }, 2000);
                        }
                    }
                }
            });
            
            deviceListener.on("ready", () => {
                console.log(`📡 Device Listener: Ready on port 9000 (receives /ipupdate from TDS-8)`);
            });
            
            deviceListener.on("error", (err) => {
                console.error("Device listener error:", err.message);
            });
            
            deviceListener.open();
        } catch (err) {
            console.error("Device listener init error:", err.message);
        }
    }
    return deviceListener;
}

// Start OSC sender and listener immediately
console.log('\n========== OSC INITIALIZATION ==========');
initOSC();        // Initialize OSC sender
initOSCListener(); // Initialize OSC listener (port 8000 - from M4L)
initDeviceListener(); // Initialize device listener (port 9000 - from TDS-8)
console.log('========================================\n');
// Function to broadcast IP update to M4L (port 9000)
function broadcastIPUpdate() {
    try {
        const port = initOSC();
        if (!port) return;
        
        const message = {
            address: "/ipupdate",
            args: [{ type: "s", value: deviceIP }]
        };
        console.log(`SENT: /ipupdate ${deviceIP} (to M4L)`);
        port.send(message, "127.0.0.1", 9000);
    } catch (err) {
        console.error("Error:", err.message);
    }
}

app.use(express.json({ limit: '10mb' }));
app.use(morgan('dev'));
app.use(express.static(path.join(__dirname, 'web')));

// Redirect old favicon.ico requests to favicon.svg
app.get('/favicon.ico', (req, res) => {
  res.redirect(301, '/favicon.svg');
});

// Explicit root route handler as fallback
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'web', 'index.html'));
});

function wsBroadcast(obj) {
  const s = JSON.stringify(obj);
  for (const c of wss.clients) if (c.readyState === 1) c.send(s);
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
    binding: Bindings, path: desiredPath, baudRate: BAUD, autoOpen: false
  });
  await new Promise((res, rej) => serial.open(err => err ? rej(err) : res()));

  serial.on('data', onSerialData);
  serial.on('error', e => wsBroadcast({ type: 'serial-error', message: e.message }));
  serial.on('close', () => wsBroadcast({ type: 'serial-close', path: serialPath }));

  wsBroadcast({ type: 'serial-open', path: serialPath, baud: BAUD });
  console.log(`Serial Port: ${serialPath} @ ${BAUD} baud`);
  send('/reannounce\n'); 
  send('VERSION\n');
  return serialPath;
}

function send(s) {
  if (!serial || !serial.isOpen) {
    console.error('❌ Cannot send - Serial not open');
    throw new Error('Serial not open');
  }
  serial.write(s);
  console.log(`SENT: ${s.trim()}`);
}

function onSerialData(chunk) {
  const text = chunk.toString('utf8');
  serialBuf += text;
  wsBroadcast({ type: 'serial-data', data: text });
  let idx;
  while ((idx = serialBuf.indexOf('\n')) >= 0) {
    const line = serialBuf.slice(0, idx).trim();
    serialBuf = serialBuf.slice(idx + 1);
    if (line) {
      console.log(`RECV: ${line}`);
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
    }
  }
}

// -------- API --------
app.get('/api/ports', async (_req, res) => {
  const ports = await listPorts();
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
      await new Promise((resolve) => {
        const cmd = 'powershell -NoProfile -Command "try { Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name, Description | ConvertTo-Json } catch { try { Get-WmiObject Win32_SerialPort | Select-Object DeviceID, Name, Description | ConvertTo-Json } catch {} }"';
        exec(cmd, { windowsHide: true }, (err, stdout) => {
          if (!err && stdout) {
            console.log('PowerShell COM port raw output:', stdout);
            try {
              const psData = JSON.parse(stdout);
              const psArray = Array.isArray(psData) ? psData : [psData];
              console.log('Parsed PowerShell data:', psArray);
              psArray.forEach(item => {
                if (item.DeviceID) {
                  console.log(`Mapping ${item.DeviceID} -> ${item.Name}`);
                  psPortInfo.set(item.DeviceID, {
                    name: item.Name || '',
                    description: item.Description || ''
                  });
                  // Add to ports list if not already there
                  if (!ports.some(p => p.path === item.DeviceID)) {
                    ports.push({
                      path: item.DeviceID,
                      manufacturer: item.Name || item.Description || '',
                      pnpId: item.Description || ''
                    });
                  }
                }
              });
            } catch (e) {
              console.log('PowerShell JSON parse error:', e.message);
              console.log('Raw stdout was:', stdout);
            }
          } else {
            console.log('PowerShell command failed or returned no output');
          }
          resolve();
        });
      });
    } catch (e) {
      console.log('PowerShell COM discovery skipped:', e.message);
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
  
  console.log(`📋 All COM ports (unfiltered): ${decorated.length} found`);
  console.log('Final port labels:', decorated.map(p => `${p.path} = "${p.label}"`).join(', '));
  res.json(decorated);
});

app.post('/api/connect', async (req, res) => {
  try {
    const desired = req.body && req.body.path;
    if (!desired) throw new Error('Missing path');
    const p = await openSerial(desired);
    // Broadcast IP update to M4L
    setTimeout(() => broadcastIPUpdate(), 500);
    res.json({ ok: true, path: p, baud: BAUD });
  } catch (e) { res.status(500).json({ error: e.message }); }
});

app.post('/api/disconnect', async (req, res) => {
  try {
    if (serial && serial.isOpen) {
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

app.post('/api/send', (req, res) => {
  try {
    let cmd = (req.body && req.body.cmd) ? String(req.body.cmd) : '';
    cmd = cmd.trim();
    if (!cmd) throw new Error('Missing cmd');
    send(cmd.endsWith('\n') ? cmd : cmd + '\n');
    res.json({ ok: true });
  } catch (e) { 
    console.error(`Error:`, e.message);
    res.status(400).json({ error: e.message }); 
  }
});

// Communication toggle
app.post('/api/comm-mode', (req, res) => {
  try {
    const mode = String((req.body && req.body.mode) || '').toLowerCase();
    if (!['wired','wifi'].includes(mode)) throw new Error('mode must be wired or wifi');
    if (mode === 'wired') {
      send('WIRED_ONLY true\n');
    } else { 
      send('WIRED_ONLY false\n'); 
      send('WIFI_ON\n'); 
    }
    res.json({ ok: true, mode });
  } catch (e) { 
    console.error(`Error:`, e.message);
    res.status(400).json({ error: e.message }); 
  }
});

// Wi-Fi join + Windows scan
app.post('/api/wifi-join', (req, res) => {
  try {
    const { ssid, password } = req.body || {};
    if (!ssid) throw new Error('Missing ssid');
    const esc = s => '"' + String(s).replace(/"/g, '\\"') + '"';
    const cmd = `WIFI_JOIN ${esc(ssid)} ${esc(password || '')}`;
    send(cmd + '\n');
    res.json({ ok: true });
  } catch (e) { 
    console.error(`Error:`, e.message);
    res.status(400).json({ error: e.message }); 
  }
});

app.get('/api/wifi-scan', (_req, res) => {
  const isMac = process.platform === 'darwin';
  const isWin = process.platform === 'win32';
  
  if (!isMac && !isWin) {
    return res.status(500).json({ error: 'WiFi scan not supported on this platform' });
  }
  
  const scanCmd = isMac 
    ? 'networksetup -listpreferredwirelessnetworks en0 2>/dev/null'
    : 'netsh wlan show networks mode=Bssid';
  
  exec(scanCmd, { windowsHide: true, timeout: 10000 }, (err, stdout) => {
    if (err) { 
      console.error('WiFi scan error:', err);
      res.status(500).json({ error: err.message }); 
      return;
    }
    
    parseWiFiOutput(stdout, isMac, res);
  });
});

function parseWiFiOutput(stdout, isMac, res) {
  console.log('WiFi scan raw output:', stdout);
  const ssids = [];
  
  if (isMac) {
    // Parse Mac networksetup output: "Preferred networks on en0:\n\t\tSSID1\n\t\tSSID2"
    stdout.split(/\r?\n/).forEach(line => {
      const trimmed = line.trim();
      if (trimmed && !trimmed.startsWith('Preferred') && !trimmed.includes(':')) {
        if (!ssids.includes(trimmed)) ssids.push(trimmed);
      }
    });
  } else {
    // Parse Windows netsh output
    stdout.split(/\r?\n/).forEach(line => {
      const m = line.match(/^\s*SSID\s+\d+\s*:\s*(.+)$/i);
      if (m) { 
        const name = m[1].trim(); 
        if (name && !ssids.includes(name)) {
          console.log(`Found SSID: "${name}"`);
          ssids.push(name); 
        }
      }
    });
  }
  
  console.log(`📡 WiFi scan complete: ${ssids.length} networks found:`, ssids);
  res.json({ ok: true, ssids });
}

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

app.post('/api/trackname', (req, res) => {
  try {
    const { index, name, actualTrack } = req.body || {};
    
    // Debug: log exactly what we received
    console.log('🔍 [DEBUG] Received trackname request:', {
      index,
      name,
      nameType: typeof name,
      nameLength: name ? name.length : 0,
      actualTrack
    });
    
    if (index === undefined || `${index}`.trim() === '') throw new Error('Missing index');
    // Allow empty string name to clear display; only require the field to be present
    if (name === undefined) throw new Error('Missing name');
    if (index >= 0 && index < 8) currentTrackNames[index] = String(name);
    
    // Build safe, quoted name and always include actualTrack (device adds +1, so default to index)
    const esc = String(name).replace(/"/g, '\\"');
    const at = (actualTrack !== undefined) ? actualTrack : Number(index);
    const cmd = `/trackname ${index} "${esc}" ${at}\n`;
    
    console.log('📤 [DEBUG] Command parts:', {
      index,
      originalName: name,
      escapedName: esc,
      actualTrack: at,
      fullCommand: cmd.trim()
    });
    
    send(cmd);
    // Persist asynchronously; ignore errors
    try { fs.promises.writeFile(TRACK_NAMES_PATH, JSON.stringify(currentTrackNames, null, 2), 'utf8'); } catch {}
    res.json({ ok: true });
  } catch (e) { 
    console.error('❌ /api/trackname error:', e);
    res.status(400).json({ error: e.message }); 
  }
});
app.post('/api/activetrack', (req, res) => {
  try {
    const { index } = req.body || {};
    if (index === undefined || `${index}`.trim() === '') throw new Error('Missing index');
    send(`/activetrack ${index}\n`);
    res.json({ ok: true });
  } catch (e) { res.status(400).json({ error: e.message }); }
});

// Send arbitrary OSC message to M4L
app.post('/api/osc-send', (req, res) => {
  try {
    const { address, args } = req.body || {};
    if (!address) throw new Error('Missing OSC address');
    
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
    
    port.send(oscMsg, "127.0.0.1", 9000);
    const logMessage = `📡 OSC sent to M4L: ${address} ${args ? args.join(' ') : ''}`;
    console.log(logMessage);
    wsBroadcast({ type: 'log', level: 'info', message: logMessage });
    
    res.json({ ok: true, message: `Sent ${address} to M4L` });
  } catch (e) { 
    console.error('OSC send error:', e.message);
    res.status(400).json({ ok: false, error: e.message }); 
  }
});

// Broadcast IP update to M4L
app.post('/api/broadcast-ip', (req, res) => {
  try {
    broadcastIPUpdate();
    res.json({ ok: true, message: 'Broadcast sent to port 9000' });
  } catch (e) { res.status(400).json({ error: e.message }); }
});

// Get current device status
app.get('/api/device-status', (req, res) => {
  const connectionType = deviceIP === '127.0.0.1' ? 'wired' : (deviceIP ? 'wifi' : 'none');
  res.json({ 
    ok: true, 
    ip: deviceIP || '—',
    connectionType: connectionType
  });
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
app.get('/api/fw-feed', async (req, res) => {
  try {
    const feedUrl = req.query.url || FW_FEED_URL;
    if (!feedUrl) throw new Error('No feed URL provided');
    const fPath = await fetchToTemp(feedUrl);
    const feed = JSON.parse(await fs.promises.readFile(fPath, 'utf8'));
    res.json({ ok: true, feed });
  } catch (e) { res.status(400).json({ error: e.message }); }
});

// Firmware — start OTA from a manifest
app.post('/api/ota-update', async (req, res) => {
  try {
    const manifestUrl = (req.body && req.body.manifest) || OTA_MANIFEST_URL;
    if (!manifestUrl) throw new Error('No manifest URL provided');
    const mPath = await fetchToTemp(manifestUrl);
    const manifest = JSON.parse(await fs.promises.readFile(mPath, 'utf8'));
    if (!manifest.url) throw new Error('Manifest missing url');
    send('WIRED_ONLY false\n');
    send('WIFI_ON\n');
    send(`OTA_URL ${manifest.url}\n`);
    res.json({ ok: true, url: manifest.url });
  } catch (e) { res.status(400).json({ error: e.message }); }
});

// -------- Utils --------
async function fetchToTemp(url) {
  const { request } = await (url.startsWith('https:') ? import('node:https') : import('node:http'));
  const tmp = path.join(require('os').tmpdir(), 'tds8-' + Date.now() + '-' + Math.random().toString(16).slice(2));
  await new Promise((resolve, reject) => {
    const req = request(url, resp => {
      if (resp.statusCode < 200 || resp.statusCode >= 300) { reject(new Error('HTTP ' + resp.statusCode)); return; }
      const f = fs.createWriteStream(tmp);
      resp.pipe(f);
      f.on('finish', () => f.close(resolve));
    });
    req.on('error', reject);
    req.end();
  });
  return tmp;
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
});

server.listen(PORT, () => {
  console.log('\n========================================================');
  console.log('         TDS-8 Bridge - Port Configuration');
  console.log('========================================================');
  console.log(`  HTTP Server:    http://localhost:${PORT}`);
  console.log('  OSC Listener:   0.0.0.0:8000 (receives from M4L)');
  console.log('  OSC Sender:     127.0.0.1:9001 -> 127.0.0.1:9000');
  console.log('  WebSocket:      ws://localhost:8088');
  console.log('========================================================');
  console.log('  M4L should send OSC to: 127.0.0.1:8000');
  console.log('  M4L should listen on:   port 9000');
  console.log('========================================================\n');
});
