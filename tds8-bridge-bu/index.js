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
const BAUD = parseInt(process.env.BAUD || '115200', 10);

// Optional defaults (you can set these later)
const OTA_MANIFEST_URL = process.env.OTA_MANIFEST_URL || '';
const FW_FEED_URL = process.env.FW_FEED_URL || '';

const Bindings = autoDetect();
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
            udpPort.open();
            console.log("📡 OSC Sender: 0.0.0.0:9001 (sends to M4L on port 9000)");
        } catch (err) {
            console.error("OSC init error:", err.message);
        }
    }
    return udpPort;
}


// OSC Listener on port 8000 for receiving commands from M4L
let oscListener = null;

function initOSCListener() {
    if (!oscListener) {
        try {
            oscListener = new osc.UDPPort({
                localAddress: "0.0.0.0",
                localPort: 8000,
                metadata: true
            });
            
            oscListener.on("message", (oscMsg, timeTag, info) => {
                const fromIP = info.address || 'unknown';
                const fromPort = info.port || 'unknown';
                console.log(`📨 OSC from ${fromIP}:${fromPort} → ${oscMsg.address}`, oscMsg.args);
                wsBroadcast({ type: 'osc-received', from: `${fromIP}:${fromPort}`, address: oscMsg.address, args: oscMsg.args });
                
                // Forward OSC commands to device via serial
                if (oscMsg.address === "/trackname" && oscMsg.args.length >= 2) {
                    const displayIndex = oscMsg.args[0].value;
                    const nameRaw = oscMsg.args[1].value;
                    const actualTrack = oscMsg.args.length >= 3 ? oscMsg.args[2].value : (displayIndex + 1);

                    const esc = String(nameRaw).replace(/"/g, '\\"');
                    const cmd = `/trackname ${displayIndex} "${esc}" ${actualTrack}\n`;
                    send(cmd);
                    console.log(`✓ Forwarded to serial: ${cmd.trim()}`);

                    // Store only string names for UI/API
                    if (displayIndex >= 0 && displayIndex < 8) {
                        currentTrackNames[displayIndex] = esc;
                    }
                }
                else if (oscMsg.address === "/activetrack" && oscMsg.args.length >= 1) {
                    const index = oscMsg.args[0].value;
                    const cmd = `/activetrack ${index}\n`;
                    send(cmd);
                    console.log(`✓ Forwarded to serial: ${cmd.trim()}`);
                }
                else if (oscMsg.address === "/ping") {
                    const cmd = `/ping\n`;
                    send(cmd);
                    console.log(`✓ Forwarded to serial: ${cmd.trim()}`);
                }
            });
            
            oscListener.on("error", (err) => {
                console.error("OSC listener error:", err.message);
            });
            
            oscListener.open();
            console.log("🎧 OSC Listener: 0.0.0.0:8000 (receiving from M4L)");
        } catch (err) {
            console.error("OSC listener init error:", err.message);
        }
    }
    return oscListener;
}

// Start OSC listener immediately
initOSCListener();
// Function to broadcast IP update to M4L (port 9000)
function broadcastIPUpdate() {
    try {
        const port = initOSC();
        if (!port) return;
        
        const message = {
            address: "/ipupdate",
            args: [{ type: "s", value: "127.0.0.1" }]
        };
        port.send(message, "127.0.0.1", 9000);
        console.log("📡 OSC Sender: 127.0.0.1:9001 → 127.0.0.1:9000 /ipupdate 127.0.0.1");
    } catch (err) {
        console.error("Broadcast error:", err.message);
    }
}

app.use(express.json({ limit: '10mb' }));
app.use(morgan('dev'));
app.use(express.static(path.join(__dirname, 'web')));

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
  console.log(`🔌 Serial Port: ${serialPath} @ ${BAUD} baud`);
  send('/reannounce\n'); send('VERSION\n');
  return serialPath;
}

function send(s) {
  if (!serial || !serial.isOpen) throw new Error('Serial not open');
  serial.write(s);
  console.log(`📤 Serial TX: ${s.trim()}`);
}

function onSerialData(chunk) {
  const text = chunk.toString('utf8');
  serialBuf += text;
  wsBroadcast({ type: 'serial-data', data: text });
  let idx;
  while ((idx = serialBuf.indexOf('\n')) >= 0) {
    const line = serialBuf.slice(0, idx).trim();
    serialBuf = serialBuf.slice(idx + 1);
    if (line) console.log(`📥 Serial RX: ${line}`);
    const m = /^VERSION\s*[:=]\s*([\w.\-]+)/i.exec(line);
    if (m) { deviceVersion = m[1]; wsBroadcast({ type: 'device-version', version: deviceVersion }); }
  }
}

// -------- API --------
app.get('/api/ports', async (_req, res) => {
  const ports = await listPorts();
  // Filter for TDS-8 devices (XIAO ESP32-C3 only)
  const tds8Devices = ports.filter(p => {
    const vid = (p.vendorId || '').toUpperCase();
    const pid = (p.productId || '').toUpperCase();
    // Only show XIAO ESP32-C3 devices
    return vid === '303A' && pid === '1001';
  });
  
  const decorated = tds8Devices.map(p => {
    return {
      path: p.path,
      label: p.path, // Just the path for the label
      vendorId: p.vendorId || '',
      productId: p.productId || '',
      serialNumber: p.serialNumber || '',
      manufacturer: p.manufacturer || '',
      pnpId: p.pnpId || ''
    };
  });
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

app.post('/api/send', (req, res) => {
  try {
    let cmd = (req.body && req.body.cmd) ? String(req.body.cmd) : '';
    cmd = cmd.trim();
    if (!cmd) throw new Error('Missing cmd');
    send(cmd.endsWith('\n') ? cmd : cmd + '\n');
    res.json({ ok: true });
  } catch (e) { res.status(400).json({ error: e.message }); }
});

// Communication toggle
app.post('/api/comm-mode', (req, res) => {
  try {
    const mode = String((req.body && req.body.mode) || '').toLowerCase();
    if (!['wired','wifi'].includes(mode)) throw new Error('mode must be wired or wifi');
    if (mode === 'wired') send('WIRED_ONLY true\n');
    else { send('WIRED_ONLY false\n'); send('WIFI_ON\n'); }
    res.json({ ok: true, mode });
  } catch (e) { res.status(400).json({ error: e.message }); }
});

// Wi-Fi join + Windows scan
app.post('/api/wifi-join', (req, res) => {
  try {
    const { ssid, password } = req.body || {};
    if (!ssid) throw new Error('Missing ssid');
    const esc = s => '"' + String(s).replace(/"/g, '\\"') + '"';
    send(`WIFI_JOIN ${esc(ssid)} ${esc(password || '')}\n`);
    res.json({ ok: true });
  } catch (e) { res.status(400).json({ error: e.message }); }
});

app.get('/api/wifi-scan', (_req, res) => {
  const isMac = process.platform === 'darwin';
  const isWin = process.platform === 'win32';
  
  if (!isMac && !isWin) {
    return res.status(500).json({ error: 'WiFi scan not supported on this platform' });
  }
  
  const cmd = isMac 
    ? 'networksetup -listpreferredwirelessnetworks en0 2>/dev/null'
    : 'netsh wlan show networks mode=Bssid';
  
  exec(cmd, { windowsHide: true }, (err, stdout) => {
    if (err) { 
      res.status(500).json({ error: err.message }); 
      return; 
    }
    
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
          if (name && !ssids.includes(name)) ssids.push(name); 
        }
      });
    }
    
    res.json({ ok: true, ssids });
  });
});

// GET /tracks - Return current track names for Ableton M4L
let currentTrackNames = [
  'Track 1', 'Track 2', 'Track 3', 'Track 4',
  'Track 5', 'Track 6', 'Track 7', 'Track 8'
];

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
    console.log('🔍 Received trackname request:', { index, name, actualTrack });
    
    if (index === undefined || `${index}`.trim() === '') throw new Error('Missing index');
    if (!name || !String(name).trim()) throw new Error('Missing name');
    if (index >= 0 && index < 8) currentTrackNames[index] = name;
    
    // Build safe, quoted name and always include actualTrack (default to index+1)
    const esc = String(name).replace(/"/g, '\\"');
    const at = (actualTrack !== undefined) ? actualTrack : (Number(index) + 1);
    const cmd = `/trackname ${index} "${esc}" ${at}\n`;
    console.log('📤 Sending command:', cmd.trim());
    send(cmd);
    res.json({ ok: true });
  } catch (e) { res.status(400).json({ error: e.message }); }
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
  console.log('\n╔════════════════════════════════════════════════════════╗');
  console.log('║           TDS-8 Bridge - Port Configuration           ║');
  console.log('╠════════════════════════════════════════════════════════╣');
  console.log(`║  HTTP Server:    http://localhost:${PORT}                  ║`);
  console.log('║  OSC Listener:   0.0.0.0:8000 (receives from M4L)     ║');
  console.log('║  OSC Sender:     127.0.0.1:9001 → 127.0.0.1:9000      ║');
  console.log('║  WebSocket:      ws://localhost:8088                   ║');
  console.log('╠════════════════════════════════════════════════════════╣');
  console.log('║  M4L should send OSC to: 127.0.0.1:8000               ║');
  console.log('║  M4L should listen on:   port 9000                    ║');
  console.log('╚════════════════════════════════════════════════════════╝\n');
});
