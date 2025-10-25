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
const OSC_LISTEN_PORT = parseInt(process.env.OSC_LISTEN_PORT || '8002', 10);  // Changed from 8000 to avoid conflicts
const BAUD = parseInt(process.env.BAUD || '115200', 10);

// Optional defaults (you can set these later)
const OTA_MANIFEST_URL = process.env.OTA_MANIFEST_URL || '';
const FW_FEED_URL = process.env.FW_FEED_URL || '';

const Bindings = autoDetect();

// Multi-device support: Store up to 4 TDS-8 devices
const MAX_DEVICES = 4;
let devices = []; // Array of { id, serial, path, buffer, version, deviceID }

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

// Send OSC message to Ableton (M4L)
function sendOSC(address, args = []) {
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
        console.log(`📤 OSC → M4L (${M4L_IP}:${M4L_PORT}): ${address}${argsStr}`);
        
        // Broadcast to browser UI
        wsBroadcast({ 
            type: 'osc-sent', 
            address: address, 
            args: args,
            argsStr: argsStr
        });
    } catch (err) {
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
                        // Send a one-time alert to all connected devices
                        sendToAll(`ALERT "Ableton Connected!" 1000\n`);
                        abletonGreetingSent = true; // Set flag to prevent re-sending
                    }
                    abletonConnected = true;
                    wsBroadcast({ type: 'ableton-connected' });
                } else {
                    if (abletonConnected) {
                        abletonConnected = false;
                        console.log('⚠️ Ableton disconnected (no /hi in 30s)');
                        wsBroadcast({ type: 'ableton-disconnected' });
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
                    // If no serial connection is open yet, ignore quietly to avoid spam on boot
                    const serialReady = (serial && serial.isOpen) || (devices && devices.length > 0);
                    if (!serialReady) {
                        // Silently ignore to prevent console spam on boot before devices are connected
                        return;
                    }
                    const displayIndex = oscMsg.args[0].value;
                    const nameRaw = oscMsg.args[1].value;
                    const actualTrack = oscMsg.args.length >= 3 ? oscMsg.args[2].value : displayIndex;

                    const esc = String(nameRaw).replace(/"/g, '\\"');
                    
                    // Multi-device: Route to correct device based on track number
                    const deviceId = Math.floor(actualTrack / 8);
                    const localIndex = actualTrack % 8;
                    const targetDevice = devices.find(d => d.id === deviceId);

                    if (targetDevice) {
                        const cmd = `/trackname ${localIndex} "${esc}" ${actualTrack}\n`;
                        sendToDevice(deviceId, cmd);
                        console.log(`📍 Routed track ${actualTrack} to Device ${deviceId} (local index ${localIndex})`);
                    } else if (devices.length > 0) {
                        // Fallback for single connected device if routing fails
                        const cmd = `/trackname ${displayIndex} "${esc}" ${actualTrack}\n`;
                        sendToDevice(devices[0].id, cmd);
                    } else {
                        // Single device mode (legacy)
                        if (serial && serial.isOpen) {
                            const cmd = `/trackname ${displayIndex} "${esc}" ${actualTrack}\n`;
                            send(cmd);
                        } else {
                            // Silently ignore if the single-device serial port isn't open
                        }
                    }

                    // Store the UNESCAPED name for UI/API (not the escaped version)
                    if (displayIndex >= 0 && displayIndex < 32) {
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
console.log('Calling initOSC()...');
const oscSender = initOSC();        // Initialize OSC sender
console.log('OSC Sender returned:', oscSender ? 'object' : 'null');

console.log('Calling initOSCListener()...');
const oscReceiver = initOSCListener(); // Initialize OSC listener (port 8000 - from M4L)
console.log('OSC Listener returned:', oscReceiver ? 'object' : 'null');

console.log('Calling initDeviceListener()...');
initDeviceListener(); // Initialize device listener (port 9000 - from TDS-8)

// Wait a moment for ports to open, then verify
setTimeout(() => {
  console.log('\n🔍 OSC Status Check:');
  console.log(`  Sender (port 9001): ${oscSender && oscSender.socket ? '✅ Ready' : '❌ Failed'}`);
  console.log(`  Listener (port 8000): ${oscReceiver && oscReceiver.socket ? '✅ Ready' : '❌ Failed'}`);
  
  if (!oscSender || !oscSender.socket) {
    console.error('⚠️ OSC Sender failed to initialize - M4L communication will not work!');
    console.error('   Check if port 9001 is already in use by another application.');
  }
  if (!oscReceiver || !oscReceiver.socket) {
    console.error('⚠️ OSC Listener failed to initialize - Cannot receive from M4L!');
    console.error('   Check if port 8000 is already in use by another application.');
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
// Function to broadcast IP update to M4L
function broadcastIPUpdate() {
    try {
        const port = initOSC();
        if (!port) return;
        
        const message = {
            address: "/ipupdate",
            args: [{ type: "s", value: deviceIP }]
        };
        console.log(`SENT: /ipupdate ${deviceIP} (to M4L on port ${M4L_PORT})`);
        port.send(message, M4L_IP, M4L_PORT);
    } catch (err) {
        console.error("Error:", err.message);
    }
}

app.use(express.json({ limit: '10mb' }));
app.use(morgan('dev'));
app.use(express.static(path.join(__dirname, 'web')));

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

// Multi-device: Send to specific device by ID
function sendToDevice(deviceId, s) {
  const device = devices.find(d => d.id === deviceId);
  if (!device || !device.serial || !device.serial.isOpen) {
    console.error(`❌ Cannot send to device ${deviceId} - Not connected`);
    throw new Error(`Device ${deviceId} not connected`);
  }
  device.serial.write(s);
  console.log(`SENT to Device ${deviceId}: ${s.trim()}`);
}

// Multi-device: Send to all connected devices
function sendToAll(s) {
  devices.forEach(device => {
    if (device.serial && device.serial.isOpen) {
      device.serial.write(s);
      console.log(`SENT to Device ${device.id}: ${s.trim()}`);
    }
  });
}

// Multi-device: Handle serial data from specific device
function onSerialDataMulti(device, chunk) {
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
  
  console.log(`📋 All COM ports (unfiltered): ${decorated.length} found`);
  console.log('Final port labels:', decorated.map(p => `${p.path} = "${p.label}"`).join(', '));
  res.json(decorated);
});

// Map COM ports to device IDs (persistent across disconnects)
const portToDeviceId = new Map();

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
    const newSerial = new SerialPortStream({ binding: Bindings, path: desired, baudRate: BAUD });
    
    const device = {
      id: deviceId,
      serial: newSerial,
      path: desired,
      buffer: '',
      version: null,
      deviceID: deviceId
    };
    
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
    if (devices.length === 1) {
      serial = newSerial;
      serialPath = desired;
      serialBuf = device.buffer;
      deviceIP = '127.0.0.1';
      
      // Broadcast IP update to M4L
      setTimeout(() => broadcastIPUpdate(), 500);
    }
    
    // Send DEVICE_ID command to set track offset
    setTimeout(() => {
      sendToDevice(deviceId, `DEVICE_ID ${deviceId}\n`);
      sendToDevice(deviceId, 'VERSION\n');
      console.log(`✓ Connected: ${desired} → Device ID ${deviceId} (Tracks ${deviceId * 8 + 1}-${(deviceId + 1) * 8})`);
    }, 1000);
    
    // Broadcast device status to UI (wired mode)
    deviceIP = '127.0.0.1';
    wsBroadcast({
      type: 'device-mode',
      mode: 'wired',
      ip: '127.0.0.1'
    });
    
    wsBroadcast({ type: 'serial-open', path: desired, baud: BAUD });
    res.json({ ok: true, path: desired, baud: BAUD, deviceId });
  } catch (e) { 
    console.error('Connection error:', e);
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
    const { ssid, password, deviceId } = req.body || {};
    if (!ssid) throw new Error('Missing ssid');
    const esc = s => '"' + String(s).replace(/"/g, '\\"') + '"';
    const cmd = `WIFI_JOIN ${esc(ssid)} ${esc(password || '')}`;
    
    // Send to specific device if deviceId provided, otherwise first device
    if (deviceId !== undefined && devices.length > 0) {
      const device = devices.find(d => d.deviceID === deviceId || d.id === deviceId);
      if (device) {
        sendToDevice(device.id, cmd + '\n');
        console.log(`📡 Sent WiFi join to Device ${deviceId}`);
      } else {
        throw new Error(`Device ${deviceId} not found`);
      }
    } else if (serial && serial.isOpen) {
      send(cmd + '\n');
    } else if (devices.length === 0) {
      throw new Error('No device connected - please connect a TDS-8 first');
    } else {
      throw new Error('Serial port not available - device may be disconnecting');
    }
    
    res.json({ ok: true });
  } catch (e) { 
    console.error(`❌ WiFi join error:`, e.message);
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
        console.log('💾 Track names saved to track_names.json');
        res.json({ ok: true });
    });
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
    
    // Calculate which device should receive this track (based on actualTrack)
    const targetDeviceId = Math.floor(at / 8);
    
    console.log('📤 [TRACKNAME] Routing:', {
      index,
      name: name,
      actualTrack: at,
      targetDeviceId,
      devicesCount: devices.length,
      availableDevices: devices.map(d => `ID=${d.deviceID}`).join(', ')
    });
    
    // Route to correct device if multi-device mode
    if (devices.length > 1) {
      console.log(`🔍 [ROUTING] Looking for deviceId=${targetDeviceId} for track ${at}`);
      const targetDevice = devices.find(d => d.deviceID === targetDeviceId || d.id === targetDeviceId);
      if (targetDevice) {
        sendToDevice(targetDevice.id, cmd);
        console.log(`📤 [Device ${targetDeviceId}] ${targetDevice.path} ← Track ${at}: "${name}"`);
      } else {
        console.warn(`⚠️ Device ${targetDeviceId} not found for track ${at}. Available:`, devices.map(d => `[ID=${d.deviceID}, path=${d.path}]`).join(', '));
        console.warn(`⚠️ Falling back to first device`);
        send(cmd);
      }
    } else {
      // Single device mode
      const firstDevice = devices.length > 0 ? devices[0] : null;
      const deviceInfo = firstDevice ? `[Device ${firstDevice.deviceID}] ${firstDevice.path}` : 'device';
      send(cmd);
      console.log(`📤 ${deviceInfo} ← Track ${at}: "${name}"`);
    }
    
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

app.post('/api/send', (req, res) => {
    try {
        const { cmd } = req.body;
        if (!cmd) throw new Error('Missing cmd');

        const upperCmd = cmd.toUpperCase();
        const broadcastCommands = ['VERSION', 'REBOOT'];

        if (broadcastCommands.includes(upperCmd)) {
            console.log(`📢 Broadcasting command to all devices: ${cmd}`);
            sendToAll(cmd + '\n');
            res.json({ ok: true, message: `Command sent to all devices.` });
        } else {
            // Default to sending to the first device for other commands
            if (devices.length > 0) {
                sendToDevice(devices[0].id, cmd + '\n');
                res.json({ ok: true, message: `Command sent to device ${devices[0].id}.` });
            } else {
                throw new Error('No devices connected to send command.');
            }
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

// Diagnostic endpoint - check OSC port status
app.get('/api/osc-status', (req, res) => {
  const now = Date.now();
  const timeSinceLastHi = lastHiTime ? now - lastHiTime : null;
  const abletonConnected = timeSinceLastHi !== null && timeSinceLastHi < 30000;
  
  const socketReady = udpPort && udpPort.socket && udpPort.socket._handle ? true : false;
  
  res.json({
    ok: true,
    oscSenderPort: 9001,
    oscListenerPort: 8000,
    deviceListenerPort: 9000,
    m4lTargetPort: 9000,
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
  console.log(`  OSC Listener:   0.0.0.0:${OSC_LISTEN_PORT} (receives from M4L)`);
  console.log(`  OSC Sender:     127.0.0.1:9001 -> 127.0.0.1:${M4L_PORT}`);
  console.log('  WebSocket:      ws://localhost:8088');
  console.log('========================================================');
  console.log(`  M4L should send OSC to: 127.0.0.1:${OSC_LISTEN_PORT}`);
  console.log(`  M4L should listen on:   port ${M4L_PORT}`);
  console.log('========================================================\n');
});
