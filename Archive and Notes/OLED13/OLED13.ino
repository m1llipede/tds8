/**********************************************************************
  TDS-8 (Track Display Strip - 8 channels)
  - 8x SSD1306 via TCA9548A (manual tcaSelect)
  - OSC: /trackname <idx> <name>, /activetrack <idx>, /reannounce
  - First run: captive portal "TDS8-Setup" (pw: tds8setup)
  - Web UI: http://tds8.local (status auto-refresh + scan/join Wi-Fi + reset + reannounce)
  - On connect: 3s splash "TDS-8 ONLINE  IP: ...", then normal track view
  - Always border; empty name => outline only; full-screen invert for active track
  - Discovery beacons: auto /ipupdate every 5s for 60s after join (stops on any UDP seen)
**********************************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFi.h>
#include <WiFiManager.h>      // tzapu/WiFiManager
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ---------- OLED / I2C ----------
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET     -1
#define I2C_ADDR        0x3C
#define TCA_ADDRESS     0x70

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- Tracks / UI ----------
const int numScreens = 8;
String trackNames[numScreens] = {
  "Track 1","Track 2","Track 3","Track 4",
  "Track 5","Track 6","Track 7","Track 8"
};
int activeTrack = -1;  // none selected initially

// ---------- Network / services ----------
Preferences prefs;                // NVS store (static IP + Wi-Fi creds)
WebServer    server(80);
WiFiUDP      Udp;

const uint16_t oscPort         = 8000;  // OSC listener
const uint16_t ipBroadcastPort = 9000;  // broadcast lease + /ipupdate
const char*   mdnsName         = "tds8"; // http://tds8.local/

// Captive-portal AP creds for first-run
const char* AP_SSID = "TDS8-Setup";
const char* AP_PW   = "tds8setup";     // set "" for open AP

// Optional static IP defaults (used by /save and /reset)
IPAddress currentIP  (192,168, 1,180);
IPAddress currentGW  (192,168, 1,  1);
IPAddress currentNM  (255,255,255,  0);
IPAddress currentDNS1(  8,  8,  8,  8);
IPAddress currentDNS2(  8,  8,  4,  4);

// User Wi-Fi creds (web-form saves these)
String wifiSSID = "";
String wifiPW   = "";

// ---------- discovery beacon timers ----------
bool        discoveryActive   = false;
unsigned long discoveryUntil  = 0;
unsigned long nextBeaconAt    = 0;
const unsigned long beaconPeriodMs = 5000;  // every 5s
const unsigned long beaconWindowMs = 60000; // for 60s

// ---------- Forward declarations ----------
void tcaSelect(uint8_t i);
void drawTrackName(uint8_t screen, const String& name);
void refreshAll();
void showNetworkSetup();               // big, simple 4-line SETUP
void showNetworkSplash(const String& ip);

void handleTrackName(OSCMessage &msg);
void handleActiveTrack(OSCMessage &msg);
void handleReannounceOSC(OSCMessage &msg);

void handleRoot();
void handleStatus();
void handleScan();
void handleSave();             // POST /save (static IP / DHCP)  [endpoint kept, UI can be added later]
void handleReset();            // POST /reset (restore static defaults)
void handleReannounceHTTP();   // POST /reannounce
void handleWifiSave();         // POST /wifi  {ssid,pw}

void saveConfig();
void loadConfig();
void saveWifiCreds();
void loadWifiCreds();
void broadcastIP();

void drawCentered(const String& s, uint8_t textSize, int y);

// =====================================================
//                      SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000); // Fast I2C

  // Init all OLEDs and draw default labels
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
      Serial.printf("❌ OLED init failed on channel %d\n", i);
    } else {
      drawTrackName(i, trackNames[i]); // default Track 1..8
    }
  }

  // Try STA join with creds saved in NVS first, then system creds
  loadWifiCreds();
  if (wifiSSID.length()) WiFi.begin(wifiSSID.c_str(), wifiPW.c_str());
  else                   WiFi.begin(); // use stored system creds if any

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 6000) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    // Launch captive portal and show BIG instructions on the displays
    showNetworkSetup();

    WiFiManager wm;
    if (!wm.autoConnect(AP_SSID, AP_PW)) {  // pass AP password here
      Serial.println("❌ Captive portal failed — restarting");
      delay(1500);
      ESP.restart();
    }
  }

  // Connected
  Serial.println("✅ Wi-Fi connected. IP: " + WiFi.localIP().toString());
  showNetworkSplash(WiFi.localIP().toString());

  // mDNS + HTTP
  if (MDNS.begin(mdnsName)) {
    Serial.println("🌐 mDNS: http://tds8.local");
  } else {
    Serial.println("⚠️ mDNS failed to start");
  }

  server.on("/",          HTTP_GET,  handleRoot);
  server.on("/status",    HTTP_GET,  handleStatus);
  server.on("/scan",      HTTP_GET,  handleScan);
  server.on("/save",      HTTP_POST, handleSave);
  server.on("/reset",     HTTP_POST, handleReset);
  server.on("/reannounce",HTTP_POST, handleReannounceHTTP);
  server.on("/wifi",      HTTP_POST, handleWifiSave);   // save SSID/PW + reconnect
  server.begin();
  Serial.println("🌐 HTTP server started");

  // OSC + initial IP broadcast (ASCII + OSC /ipupdate)
  Udp.begin(oscPort);
  broadcastIP();

  // Start discovery beacons for the first minute
  discoveryActive  = true;
  discoveryUntil   = millis() + beaconWindowMs;
  nextBeaconAt     = millis() + beaconPeriodMs;
}

// =====================================================
//                       LOOP
// =====================================================
void loop() {
  server.handleClient();

  // Handle OSC
  OSCMessage msg;
  int size = Udp.parsePacket();
  if (size > 0) {
    while (size--) msg.fill(Udp.read());
    if (!msg.hasError()) {
      msg.dispatch("/trackname",   handleTrackName);
      msg.dispatch("/activetrack", handleActiveTrack);
      msg.dispatch("/reannounce",  handleReannounceOSC);
    }
    // any UDP activity means the host likely found us → stop beacons
    discoveryActive = false;
  }

  // Discovery beacons (auto /ipupdate) for a short window after join
  unsigned long now = millis();
  if (discoveryActive) {
    if (now >= discoveryUntil) {
      discoveryActive = false;
    } else if (now >= nextBeaconAt) {
      broadcastIP();
      nextBeaconAt += beaconPeriodMs;
    }
  }
}

// =====================================================
//                 OLED & TCA9548A helpers
// =====================================================
void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);           // select channel i
  Wire.endTransmission();
}

void drawTrackName(uint8_t screen, const String& name) {
  tcaSelect(screen);
  display.invertDisplay(screen == activeTrack);  // full-screen invert for active
  display.clearDisplay();

  // Border always
  display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

  // Centered label if non-empty
  String trimmed = name; trimmed.trim();
  if (trimmed.length() > 0) {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(trimmed, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w)/2, (SCREEN_HEIGHT - h)/2);
    display.println(trimmed);
  }

  display.display();
}

void refreshAll() {
  for (uint8_t i = 0; i < numScreens; i++) drawTrackName(i, trackNames[i]);
}

void drawCentered(const String& s, uint8_t textSize, int y) {
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.println(s);
}

// =====================================================
//                OLED helper screens
// =====================================================
void showNetworkSetup() {
  // Big, simple 4-line page:
  // SETUP
  // Join: TDS8-Setup
  // PW:   tds8setup
  // Go to: tds8.local
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

    const int lines = 4;
    const int lineH = 16;                // approx for size=2
    int startY = (SCREEN_HEIGHT - lines * lineH) / 2; // vertical centering

    drawCentered("SETUP", 2, startY + 0*lineH);
    drawCentered(String("Join: ") + AP_SSID, 2, startY + 1*lineH);
    drawCentered(String("PW: ") + AP_PW,     2, startY + 2*lineH);
    drawCentered("Go to: tds8.local", 2,     startY + 3*lineH);

    display.display();
  }
}

void showNetworkSplash(const String& ip) {
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    drawCentered("TDS-8 ONLINE", 2, 14);
    drawCentered(String("IP: ") + ip, 2, 36);
    display.display();
  }
  delay(3000);
  refreshAll();
}

// =====================================================
//                     OSC handlers
// =====================================================
void handleTrackName(OSCMessage &msg) {
  if (msg.size() < 2) return;
  int idx = msg.getInt(0);
  char buf[64]; msg.getString(1, buf, sizeof(buf));
  if (idx >= 0 && idx < numScreens) {
    trackNames[idx] = String(buf);
    drawTrackName(idx, trackNames[idx]);
    Serial.printf("✅ /trackname %d '%s'\n", idx, buf);
  }
}

void handleActiveTrack(OSCMessage &msg) {
  if (msg.size() < 1) return;
  int idx = msg.getInt(0);
  if (idx < 0 || idx >= numScreens || idx == activeTrack) return;
  int old = activeTrack;
  activeTrack = idx;
  if (old >= 0) drawTrackName(old, trackNames[old]);
  drawTrackName(activeTrack, trackNames[activeTrack]);
  Serial.printf("🎯 /activetrack %d\n", idx);
}

void handleReannounceOSC(OSCMessage &msg) { broadcastIP(); }

// =====================================================
//                     HTTP endpoints
// =====================================================

void handleRoot() {
  // Styled UI (status auto-refresh every 2s; no manual Refresh button)
  String html = R"rawliteral(
<!DOCTYPE html><html lang="en"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>TDS-8 Network</title>
<style>
:root{--brand:#ff8800;--brand2:#8a2be2;--text:#222;--muted:#666;--card:#fff;--bg:#f6f7fb}
*{box-sizing:border-box}body{margin:0;background:var(--bg);font:15px/1.4 system-ui,Segoe UI,Roboto}
.wrap{max-width:800px;margin:40px auto;padding:0 16px}
.card{background:var(--card);border-radius:14px;box-shadow:0 10px 24px rgba(0,0,0,.08);overflow:hidden;margin-bottom:16px}
.hdr{background:linear-gradient(135deg,var(--brand),var(--brand2));padding:18px 20px;color:#fff}
.hdr h1{margin:0;font-size:20px;font-weight:600}
.body{padding:18px 20px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
label{display:block;color:var(--muted);font-size:12px;margin-bottom:4px}
.value,.input{padding:10px 12px;border:1px solid #e3e6ef;border-radius:10px;background:#fafbff}
.input{background:#fff}
.actions{display:flex;gap:10px;margin-top:14px;flex-wrap:wrap}
.btn{appearance:none;border:0;border-radius:10px;padding:10px 14px;cursor:pointer}
.btn.primary{background:var(--brand);color:#fff}
.btn.ghost{background:#fff;border:1px solid #e3e6ef}
.note{margin-top:8px;color:var(--muted)}
.kicker{margin-top:16px;font-size:13px;color:var(--muted)}
small{color:var(--muted)}
</style></head><body>
<div class="wrap">

  <div class="card">
    <div class="hdr"><h1>TDS-8 Network</h1></div>
    <div class="body">
      <div class="grid">
        <div><label>IP Address</label><div class="value" id="ip">–</div></div>
        <div><label>Gateway</label><div class="value" id="gw">–</div></div>
        <div><label>Subnet</label><div class="value" id="nm">–</div></div>
        <div><label>DNS 1</label><div class="value" id="dns1">–</div></div>
        <div><label>DNS 2</label><div class="value" id="dns2">–</div></div>
      </div>
      <div class="actions">
        <button class="btn primary" id="reannounce">Reannounce (broadcast IP)</button>
        <button class="btn ghost"   id="copy">Copy IP</button>
        <button class="btn ghost"   id="factory">Factory Reset</button>
      </div>
      <div class="kicker">M4L tip: listen on UDP 9000 for <code>/ipupdate</code> to auto-fill your destination.</div>
      <div id="msg" class="note"></div>
    </div>
  </div>

  <div class="card">
    <div class="hdr"><h1>Join a Different Wi-Fi</h1></div>
    <div class="body">
      <div class="grid">
        <div>
          <label>SSID <small>(type or pick)</small></label>
          <input class="input" list="ssidlist" id="ssid" placeholder="Network name"/>
          <datalist id="ssidlist"></datalist>
        </div>
        <div>
          <label>Password</label>
          <input class="input" id="pw" type="password" placeholder="••••••••"/>
        </div>
      </div>
      <div class="actions">
        <button class="btn primary" id="scan">Scan Networks</button>
        <button class="btn ghost"   id="connect">Connect & Save</button>
      </div>
      <div class="note">Device will reconnect; page may stop responding ~10–20s, then come back on the new network/IP.</div>
    </div>
  </div>

</div>
<script>
const F=id=>document.getElementById(id);
async function loadStatus(){
  try{
    const r=await fetch('/status'); const j=await r.json();
    ['ip','gw','nm','dns1','dns2'].forEach(k=>F(k).textContent=j[k]||'–');
  }catch(e){/* ignore */}
}
async function post(url,body){
  try{ const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{})});
       return await r.text(); }catch(e){ return 'error'; }
}
F('copy').onclick = ()=>{navigator.clipboard.writeText(F('ip').textContent);};
F('reannounce').onclick=async()=>{F('msg').textContent='Broadcasting…';await post('/reannounce');F('msg').textContent='Broadcast sent.';setTimeout(()=>F('msg').textContent='',1200);}
F('factory').onclick=async()=>{if(confirm('Factory reset static IP?')){F('msg').textContent='Resetting…';await post('/reset');}}

F('scan').onclick=async()=>{
  F('msg').textContent='Scanning…';
  try{ const r=await fetch('/scan'); const arr=await r.json();
       F('ssidlist').innerHTML = arr.map(o=>`<option value="${o.ssid}">`).join('');
       F('msg').textContent='Scan complete.';
  }catch(e){F('msg').textContent='Scan failed.'}
  setTimeout(()=>F('msg').textContent='',1500);
};
F('connect').onclick=async()=>{
  const ssid=F('ssid').value.trim(), pw=F('pw').value;
  if(!ssid){F('msg').textContent='Enter SSID'; return;}
  F('msg').textContent='Connecting…';
  await post('/wifi',{ssid,pw});
};

loadStatus();
setInterval(loadStatus, 2000);  // auto-refresh status every 2s
</script>
</body></html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleStatus() {
  DynamicJsonDocument doc(256);
  doc["ip"]   = WiFi.localIP().toString();
  doc["gw"]   = WiFi.gatewayIP().toString();
  doc["nm"]   = WiFi.subnetMask().toString();
  doc["dns1"] = WiFi.dnsIP(0).toString();
  doc["dns2"] = WiFi.dnsIP(1).toString();
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["enc"]  = WiFi.encryptionType(i);
  }
  WiFi.scanDelete();
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// POST /save — configure static IP (optional) or DHCP if omitted
void handleSave() {
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"result\":\"error\",\"msg\":\"bad json\"}");
    return;
  }
  String ipS = doc["ip"]   | "";
  String gwS = doc["gw"]   | "";
  String nmS = doc["nm"]   | "";
  String d1S = doc["dns1"] | "";
  String d2S = doc["dns2"] | "";

  if (ipS.length()) currentIP.fromString(ipS);
  if (gwS.length()) currentGW.fromString(gwS);
  if (nmS.length()) currentNM.fromString(nmS);
  if (d1S.length()) currentDNS1.fromString(d1S);
  if (d2S.length()) currentDNS2.fromString(d2S);
  saveConfig();

  WiFi.disconnect(true);
  if (ipS.length()) WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  else              WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // DHCP

  WiFi.begin(); while (WiFi.status() != WL_CONNECTED) delay(200);
  broadcastIP();
  server.send(200,"application/json","{\"result\":\"ok\"}");
}

void handleReset() {
  currentIP   = IPAddress(192,168,1,180);
  currentGW   = IPAddress(192,168,1,1);
  currentNM   = IPAddress(255,255,255,0);
  currentDNS1 = IPAddress(8,8,8,8);
  currentDNS2 = IPAddress(8,8,4,4);
  saveConfig();

  WiFi.disconnect(true);
  WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  WiFi.begin(); while (WiFi.status() != WL_CONNECTED) delay(200);
  broadcastIP();
  server.send(200,"application/json","{\"result\":\"reset\"}");
}

void handleReannounceHTTP(){ broadcastIP(); server.send(200,"application/json","{\"result\":\"ok\"}"); }

// POST /wifi {ssid,pw} — save creds and reconnect
void handleWifiSave() {
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"bad json\"}");
    return;
  }
  String ssid = doc["ssid"] | "";
  String pw   = doc["pw"]   | "";
  if (!ssid.length()) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"missing ssid\"}");
    return;
  }

  wifiSSID = ssid; wifiPW = pw;
  saveWifiCreds();

  WiFi.disconnect(true);
  WiFi.begin(wifiSSID.c_str(), wifiPW.c_str());

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 15000) delay(250);

  if (WiFi.status() == WL_CONNECTED) {
    showNetworkSplash(WiFi.localIP().toString());
    broadcastIP();
    // restart discovery beacons after a network change
    discoveryActive = true;
    discoveryUntil  = millis() + beaconWindowMs;
    nextBeaconAt    = millis() + beaconPeriodMs;
    server.send(200,"application/json","{\"result\":\"ok\"}");
  } else {
    server.send(200,"application/json","{\"result\":\"fail\"}");
  }
}

// =====================================================
//                NVS save / load helpers
// =====================================================
void saveConfig() {
  prefs.begin("net", false);
  prefs.putString("ip",   currentIP.toString());
  prefs.putString("gw",   currentGW.toString());
  prefs.putString("nm",   currentNM.toString());
  prefs.putString("dns1", currentDNS1.toString());
  prefs.putString("dns2", currentDNS2.toString());
  prefs.end();
}
void loadConfig() {
  prefs.begin("net", true);
  currentIP  .fromString(prefs.getString("ip",   currentIP.toString()));
  currentGW  .fromString(prefs.getString("gw",   currentGW.toString()));
  currentNM  .fromString(prefs.getString("nm",   currentNM.toString()));
  currentDNS1.fromString(prefs.getString("dns1", currentDNS1.toString()));
  currentDNS2.fromString(prefs.getString("dns2," currentDNS2.toString()));
  prefs.end();
}
void saveWifiCreds(){
  prefs.begin("wifi", false);
  prefs.putString("ssid", wifiSSID);
  prefs.putString("pw",   wifiPW);
  prefs.end();
}
void loadWifiCreds(){
  prefs.begin("wifi", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPW   = prefs.getString("pw",   "");
  prefs.end();
}

// =====================================================
//           Broadcast current IP (ASCII + OSC)
// =====================================================
void broadcastIP() {
  IPAddress gw = WiFi.gatewayIP();
  IPAddress bcast(gw[0], gw[1], gw[2], 255);
  String s = WiFi.localIP().toString();

  // ASCII
  Udp.beginPacket(bcast, ipBroadcastPort);
  Udp.print(s);
  Udp.endPacket();

  // OSC /ipupdate "<ip>"
  OSCMessage m("/ipupdate");
  m.add(s.c_str());
  Udp.beginPacket(bcast, ipBroadcastPort);
  m.send(Udp);
  Udp.endPacket();

  Serial.printf("🔊 Announce: %s → %s:%d\n",
                s.c_str(), bcast.toString().c_str(), ipBroadcastPort);
}
