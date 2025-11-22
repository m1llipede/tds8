/*
  TDS-8 v1.28 — Wired-first with optional Wi-Fi, Rescue AP, OTA_URL support,
  colorful on-device dashboard, and OLED onboarding:
    - OLED shows ONLY wired-mode tips (base+rotate) while in wiredOnly=true
    - When switching to Wi-Fi, OLED onboarding is disabled (no Wi-Fi tips on OLED)
    - Web dashboard shows a live “Wi-Fi Tips” card (IP + next steps) when connected

  Target board: Seeed XIAO ESP32-C3
*/

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- Version ----------------
#define FW_VERSION "1.28"

// ---------------- Display (adjust if needed) ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- Timers & Rescue AP ----------------
static const uint32_t WIFI_CONNECT_WINDOW_MS = 20000;                // 20s to connect STA
static const uint32_t RESCUE_AP_TIMEOUT_MS   = 5UL * 60UL * 1000UL;  // 5 minutes
static const char*    RESCUE_AP_SSID         = "TDS8-Rescue";
static const char*    RESCUE_AP_PASS         = "tds8setup";

// ---------------- NVS keys ----------------
static const char* PREF_NS    = "tds8";
static const char* PREF_SSID  = "ssid";
static const char* PREF_PASS  = "pass";
static const char* PREF_WIRED = "wired";   // 1 = wiredOnly, 0 = Wi-Fi enabled

// ---------------- Globals (single definitions) ----------------
bool wiredOnly = true;   // default to wired; Wi-Fi can be enabled via serial/web
Preferences prefs;
String savedSsid, savedPass;

// Rescue AP state
bool     rescueApActive    = false;
uint32_t rescueApStartedAt = 0;

// On-device web
WebServer server(80);

// ---------------- OLED Intro (Wired only) ----------------
// Base slide (#0) stays for INTRO_SLIDE_MS, then rotate slides #1 and #2 every INTRO_SLIDE_MS.
// OLED intro is active ONLY while wiredOnly==true.
static const uint32_t INTRO_SLIDE_MS = 15000; // 15 seconds
bool     introEnabled    = true;   // auto-managed by mode; HIDE_INTRO/SHOW_INTRO override
bool     introBaseLocked = true;   // true -> show base slide (#0) for 15s before rotating
uint8_t  introCycleIdx   = 1;      // rotates 1 <-> 2
uint32_t introT0         = 0;

// ---------------- Forward declarations ----------------
static void handleSerialLine(String line);
static void applyWiredOnly(bool on);
static void startRescueAP();
static void stopRescueAP();
static bool beginSTAFromPrefs(uint32_t windowMs);
static void saveWifi(const String& ssid, const String& pass);
static void forgetWifi();
static void beginWeb();
static bool performHttpUpdate(const String& url);

// OLED helpers
static void oledRenderLines(const char* a=nullptr, const char* b=nullptr, const char* c=nullptr, const char* d=nullptr);
static void oledShowWiredSlide(uint8_t idx);
static void oledShowBaseSlide();
static void oledShowCycleSlide(uint8_t idx12);
static void oledIntroReset();
static void oledIntroTick();

// ---------------- On-device HTML UI (colorful, minimal) ----------------
static const char PAGE_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>TDS-8 Network</title>
<style>
:root{--brand:#ff8800;--brand2:#8a2be2;--text:#222;--muted:#666;--card:#fff;--bg:#f6f7fb}
*{box-sizing:border-box}body{margin:0;background:var(--bg);font:15px/1.4 system-ui,Segoe UI,Roboto}
.wrap{max-width:820px;margin:32px auto;padding:0 16px}
.card{background:var(--card);border-radius:14px;box-shadow:0 10px 24px rgba(0,0,0,.08);overflow:hidden;margin-bottom:16px}
.hdr{background:linear-gradient(135deg,var(--brand),var(--brand2));padding:16px 20px;color:#fff}
.hdr h1{margin:0;font-size:20px;font-weight:600}
.body{padding:16px 20px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
label{display:block;color:var(--muted);font-size:12px;margin-bottom:4px}
.input,select{padding:10px 12px;border:1px solid #e3e6ef;border-radius:10px;background:#fff;width:100%}
.actions{display:flex;gap:10px;margin-top:12px;flex-wrap:wrap}
.btn{appearance:none;border:0;border-radius:10px;padding:10px 14px;cursor:pointer}
.btn.primary{background:var(--brand);color:#fff}
.btn.ghost{background:#fff;border:1px solid #e3e6ef}
.small{font-size:12px;color:var(--muted)}
.hr{height:1px;background:#eceff6;margin:12px 0}
.log{font-family:ui-monospace,Menlo,Consolas,monospace;background:#fafbff;border:1px solid #e3e6ef;border-radius:10px;padding:10px;min-height:140px;white-space:pre-wrap;overflow:auto}
.trackgrid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.trackrow{display:grid;grid-template-columns:60px 1fr;gap:8px;align-items:center}
.hidden{display:none}
</style></head><body>
<div class=wrap>

<div class=card>
  <div class=hdr><h1>TDS-8 Dashboard <span class=small>— Firmware: v1.28</span></h1></div>
  <div class=body>
    <div class=small>
      • Defaults to Wired (USB). Use the desktop bridge for control.<br>
      • If Wi-Fi fails to connect within 20s after enabling, a Rescue AP starts:
        SSID: TDS8-Rescue  PW: tds8setup  (auto-stops on connect or after 5 min)
    </div>
  </div>
</div>

<!-- Wi-Fi Tips (shown only when Wi-Fi is connected) -->
<div class=card id=wifiCard classx="hidden">
  <div class=hdr><h1>Wi-Fi Tips</h1></div>
  <div class=body>
    <div class=small id=wifiStatus>Checking…</div>
    <div class=hr></div>
    <div class=small>
      • Keep the desktop bridge open for serial-over-USB control if needed.<br>
      • To set track names quickly, use “Send All” below.<br>
      • OTA: the bridge can push a manifest or direct .bin via OTA_URL.
    </div>
  </div>
</div>

<div class=card>
  <div class=hdr><h1>Communication</h1></div>
  <div class=body>
    <label>Mode</label>
    <div class=actions>
      <button class="btn ghost" onclick="fetch('/api/comm?mode=wired').then(()=>log('Mode: Wired'))">Wired</button>
      <button class="btn ghost" onclick="fetch('/api/comm?mode=wifi').then(()=>log('Mode: Wi-Fi enabled'))">Wi-Fi Enabled</button>
    </div>
    <div class=hr></div>
    <div class=grid>
      <div>
        <label>Wi-Fi SSID</label>
        <input id=ssid class=input placeholder="e.g. StudioNet">
      </div>
      <div>
        <label>Password</label>
        <input id=pw class=input type=password placeholder="e.g. mysecret123">
      </div>
    </div>
    <div class=actions>
      <button class="btn primary" onclick="join()">Join</button>
      <button class="btn ghost" onclick="fetch('/api/forget').then(()=>log('Forgot Wi-Fi'))">Forget Wi-Fi</button>
    </div>
    <div class=small>Join format sent: WIFI_JOIN "SSID" "PASSWORD"</div>
  </div>
</div>

<div class=card>
  <div class=hdr><h1>Commands</h1></div>
  <div class=body>
    <div class=grid>
      <input id=raw class=input placeholder="Raw, e.g.  /ping  or  VERSION  or  WIRED_ONLY false">
      <button class="btn ghost" onclick="sendRaw()">Send</button>
    </div>
    <div class=grid style="margin-top:10px">
      <input id=active class=input placeholder="Active track index (example: 5)">
      <button class="btn ghost" onclick="sendActive()">Send</button>
    </div>
    <div class=small>Active track format:  /activetrack &lt;index&gt;</div>
    <div class=hr></div>

    <label>Batch Track Names (1–8 shown, sends indices 0–7)</label>
    <div class=trackgrid id=tracks></div>
    <div class=actions>
      <button class="btn primary" onclick="sendAll()">Send All</button>
      <div class=small>Each row sends “/trackname &lt;index&gt; &lt;name&gt;”, e.g. “/trackname 0 Ambient Lead”.</div>
    </div>
  </div>
</div>

<div class=card>
  <div class=hdr><h1>Firmware</h1></div>
  <div class=body>
    <div class=grid>
      <input id=manifest class=input placeholder="Manifest URL (optional — desktop bridge handles latest)">
      <button class="btn ghost" onclick="check()">Check</button>
    </div>
    <div class=actions>
      <button class="btn primary" onclick="startOta()">Start Update</button>
      <div id=ota class=small>Idle</div>
    </div>
    <div class=small>Update sends: WIRED_ONLY false → WIFI_ON → OTA_URL &lt;bin URL&gt;.</div>
  </div>
</div>

<div class=card>
  <div class=hdr><h1>Serial Log</h1></div>
  <div class=body><div id=log class=log></div></div>
</div>

</div>
<script>
function log(s){ const d=document.createElement('div'); d.textContent=s; const l=document.getElementById('log'); l.appendChild(d); l.scrollTop=l.scrollHeight; }
function sendRaw(){ const v=document.getElementById('raw').value.trim(); if(!v){log('Missing cmd');return;} fetch('/api/send?cmd='+encodeURIComponent(v)).then(()=>log('> '+v)); }
function sendActive(){ const v=document.getElementById('active').value.trim(); if(!v){log('Enter index');return;} fetch('/api/send?cmd='+encodeURIComponent('/activetrack '+v)).then(()=>log('> /activetrack '+v)); }
function row(id,i){ return '<div class=trackrow><div style="text-align:center;border:1px solid #e3e6ef;border-radius:10px;padding:10px 12px;background:#fafbff">'+(i+1)+'</div><input id="'+id+'" class=input placeholder="Name for track '+(i+1)+' (sends '+i+')"></div>'; }
(function(){ let s=''; for(let i=0;i<8;i++){ s+=row('tname'+i,i);} document.getElementById('tracks').innerHTML=s; })();
async function sendAll(){ for(let i=0;i<8;i++){ const v=(document.getElementById('tname'+i).value||'').trim(); if(v) await fetch('/api/send?cmd='+encodeURIComponent('/trackname '+i+' '+v)); } log('Sent all non-empty /trackname entries'); }
function join(){ const ssid=document.getElementById('ssid').value.trim(); const pw=document.getElementById('pw').value.trim(); if(!ssid){log('Enter SSID');return;} fetch('/api/join?ssid='+encodeURIComponent(ssid)+'&pw='+encodeURIComponent(pw)).then(()=>log('Join sent')); }
async function check(){ const m=document.getElementById('manifest').value.trim(); if(!m){log('Enter manifest URL');return;} const r=await fetch('/api/check?manifest='+encodeURIComponent(m)); const j=await r.json(); document.getElementById('ota').textContent = j.msg||'Checked'; log(j.msg||'Checked'); }
async function startOta(){ const m=document.getElementById('manifest').value.trim(); if(!m){log('Enter manifest URL or push from desktop');return;} const r=await fetch('/api/ota?manifest='+encodeURIComponent(m)); const j=await r.json(); document.getElementById('ota').textContent = j.msg||'OTA started'; log(j.msg||'OTA started'); }

// Wi-Fi status poll: show tips card only when connected
async function pollState(){
  try{
    const r = await fetch('/api/state'); const j = await r.json();
    const card = document.getElementById('wifiCard');
    const lbl  = document.getElementById('wifiStatus');
    if (j.wifiConnected){
      card.className = 'card';
      lbl.textContent = 'Connected: ' + (j.ip||'') + '   Hostname: ' + (j.hostname||'');
    } else {
      card.className = 'card hidden';
    }
  }catch(e){}
  setTimeout(pollState, 2000);
}
pollState();
</script>
</body></html>
)rawliteral";

// ---------------- Web handlers ----------------
static void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", PAGE_INDEX);
}
static void handleSend() {
  String cmd = server.arg("cmd");
  if (cmd.length()) handleSerialLine(cmd);
  server.send(200, "application/json", "{\"ok\":true}");
}
static void handleComm() {
  String mode = server.arg("mode");
  if (mode == "wired")      applyWiredOnly(true);
  else if (mode == "wifi")  applyWiredOnly(false);
  server.send(200, "application/json", "{\"ok\":true}");
}
static void handleJoin() {
  String ssid = server.arg("ssid");
  String pw   = server.arg("pw");
  if (ssid.length()) handleSerialLine("WIFI_JOIN \"" + ssid + "\" \"" + pw + "\"");
  server.send(200, "application/json", "{\"ok\":true}");
}
static void handleForget() {
  handleSerialLine("FORGET");
  server.send(200, "application/json", "{\"ok\":true}");
}
static void handleCheck() {
  String manifest = server.arg("manifest");
  String msg = manifest.length() ? "Manifest set" : "No manifest";
  server.send(200, "application/json", String("{\"ok\":true,\"msg\":\"") + msg + "\"}");
}
static void handleOta() {
  // Accept manifest.json with "url" to .bin; forwards OTA_URL <bin>
  String manifest = server.arg("manifest");
  if (manifest.length()) {
    HTTPClient http;
    http.begin(manifest);
    int code = http.GET();
    if (code == 200) {
      String body = http.getString();
      int vi = body.indexOf("\"url\"");
      if (vi >= 0) {
        int q1 = body.indexOf('"', vi+4);
        q1 = body.indexOf('"', q1+1);
        int q2 = body.indexOf('"', q1+1);
        String url = body.substring(q1+1, q2);
        handleSerialLine("OTA_URL " + url);
        server.send(200, "application/json", "{\"ok\":true,\"msg\":\"OTA kicked\"}");
        http.end();
        return;
      }
    }
    http.end();
  }
  server.send(200, "application/json", "{\"ok\":false,\"msg\":\"No/invalid manifest\"}");
}
static void handleState() {
  bool wifiOn = (WiFi.getMode() != WIFI_OFF);
  bool wifiConnected = (wifiOn && WiFi.status() == WL_CONNECTED);
  IPAddress ip = wifiConnected ? WiFi.localIP() : IPAddress(0,0,0,0);
  String ipStr = wifiConnected ? ip.toString() : "";
  String host = WiFi.getHostname() ? String(WiFi.getHostname()) : "";
  String json = String("{\"ok\":true,")
    + "\"wiredOnly\":" + (wiredOnly ? "true" : "false") + ","
    + "\"version\":\"" + FW_VERSION + "\","
    + "\"wifiOn\":" + (wifiOn ? "true" : "false") + ","
    + "\"wifiConnected\":" + (wifiConnected ? "true" : "false") + ","
    + "\"ip\":\"" + ipStr + "\","
    + "\"hostname\":\"" + host + "\"}";
  server.send(200, "application/json", json);
}
static void beginWeb() {
  server.on("/",           HTTP_GET, handleRoot);
  server.on("/api/send",   HTTP_ANY, handleSend);
  server.on("/api/comm",   HTTP_ANY, handleComm);
  server.on("/api/join",   HTTP_ANY, handleJoin);
  server.on("/api/forget", HTTP_ANY, handleForget);
  server.on("/api/check",  HTTP_ANY, handleCheck);
  server.on("/api/ota",    HTTP_ANY, handleOta);
  server.on("/api/state",  HTTP_GET, handleState);
  server.begin();
}

// ---------------- Wi-Fi helpers ----------------
static void applyWiredOnly(bool on) {
  wiredOnly = on;
  prefs.putUChar(PREF_WIRED, wiredOnly ? 1 : 0);
  if (wiredOnly) {
    stopRescueAP();
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    Serial.println("OK: WIRED_ONLY true");

    // OLED intro ON in wired mode
    introEnabled = true;
    oledIntroReset();
  } else {
    WiFi.mode(WIFI_STA);
    Serial.println("OK: WIRED_ONLY false");
    Serial.println("OK: WIFI_ON");
    if (!beginSTAFromPrefs(WIFI_CONNECT_WINDOW_MS)) startRescueAP();

    // OLED intro OFF in Wi-Fi mode (per your latest request)
    introEnabled = false;
  }
}
static void startRescueAP() {
  if (rescueApActive) return;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(RESCUE_AP_SSID, RESCUE_AP_PASS);
  rescueApActive = true;
  rescueApStartedAt = millis();
  Serial.println("RESCUE_AP: started");
}
static void stopRescueAP() {
  if (!rescueApActive) return;
  WiFi.softAPdisconnect(true);
  rescueApActive = false;
  Serial.println("RESCUE_AP: stopped");
}
static void saveWifi(const String& ssid, const String& pass) {
  prefs.putString(PREF_SSID, ssid);
  prefs.putString(PREF_PASS, pass);
  savedSsid = ssid; savedPass = pass;
}
static void forgetWifi() {
  prefs.remove(PREF_SSID);
  prefs.remove(PREF_PASS);
  savedSsid = ""; savedPass = "";
}
static bool beginSTAFromPrefs(uint32_t windowMs) {
  savedSsid = prefs.getString(PREF_SSID, "");
  savedPass = prefs.getString(PREF_PASS, "");
  if (savedSsid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSsid.c_str(), savedPass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < windowMs) {
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

// ---------------- OTA from direct URL ----------------
static bool performHttpUpdate(const String& url) {
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  int len = http.getSize();
  WiFiClient * stream = http.getStreamPtr();

  if (!Update.begin(len > 0 ? len : UPDATE_SIZE_UNKNOWN)) { http.end(); return false; }
  size_t written = Update.writeStream(*stream);
  bool ok = (written > 0 && Update.end());
  http.end();

  if (ok && Update.isFinished()) {
    Serial.println("OK: OTA_DONE");
    delay(200);
    ESP.restart();
  } else {
    Serial.println("ERR: OTA_FAIL");
  }
  return ok;
}

// ---------------- OLED helpers & intro logic (wired-only) ----------------
static void oledRenderLines(const char* a, const char* b, const char* c, const char* d) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  int16_t y = 0;
  auto line = [&](const char* s){
    if (!s) return;
    display.setCursor(0, y);
    display.println(s);
    y += 16;
  };
  line(a); line(b); line(c); line(d);
  display.display();
}

// Wired slides: 0=base, 1,2 rotate
static void oledShowWiredSlide(uint8_t idx) {
  switch (idx % 3) {
    case 0:
      oledRenderLines(
        "TDS-8 v" FW_VERSION,
        "Wired (USB) mode",
        "Open desktop bridge:",
        "http://localhost:8088"
      ); break;
    case 1:
      oledRenderLines(
        "USB quick test:",
        "/ping , VERSION",
        "/activetrack N",
        "/trackname 0 Name"
      ); break;
    default: // 2
      oledRenderLines(
        "Wi-Fi later? Use:",
        "WIRED_ONLY false",
        "then WIFI_ON",
        "or desktop Join"
      ); break;
  }
}

static void oledShowBaseSlide() {
  oledShowWiredSlide(0);
}
static void oledShowCycleSlide(uint8_t idx12) {
  oledShowWiredSlide(idx12);
}
static void oledIntroReset() {
  introBaseLocked = true;   // pin base slide for 15s
  introCycleIdx   = 1;      // next rotation starts at slide 1
  introT0         = millis();
  if (introEnabled) oledShowBaseSlide();
}
static void oledIntroTick() {
  // Run only if intro is enabled AND we are in wired mode
  if (!introEnabled || !wiredOnly) return;

  uint32_t now = millis();
  if (introBaseLocked) {
    if (now - introT0 >= INTRO_SLIDE_MS) {
      introBaseLocked = false;
      introT0 = now;
      oledShowCycleSlide(introCycleIdx); // draw first rotating slide
    }
    return;
  }
  if (now - introT0 >= INTRO_SLIDE_MS) {
    introT0 = now;
    introCycleIdx = (introCycleIdx == 1) ? 2 : 1; // toggle 1<->2
    oledShowCycleSlide(introCycleIdx);
  }
}

// ---------------- Serial command handler ----------------
static void handleSerialLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line.equalsIgnoreCase("VERSION")) {
    Serial.println(String("VERSION: ") + FW_VERSION);
    return;
  }

  if (line.equalsIgnoreCase("WIFI_ON") || line.equalsIgnoreCase("WIFI")) {
    applyWiredOnly(false);
    return;
  }
  if (line.equalsIgnoreCase("WIRED_ONLY true"))  { applyWiredOnly(true);  return; }
  if (line.equalsIgnoreCase("WIRED_ONLY false")) { applyWiredOnly(false); return; }

  if (line.startsWith("WIFI_JOIN")) {
    // Expect: WIFI_JOIN "SSID" "PASS"
    int q1 = line.indexOf('"'); int q2 = line.indexOf('"', q1+1);
    int q3 = line.indexOf('"', q2+1); int q4 = line.indexOf('"', q3+1);
    String ssid = (q1>=0 && q2>q1) ? line.substring(q1+1,q2) : "";
    String pass = (q3>=0 && q4>q3) ? line.substring(q3+1,q4) : "";
    if (ssid.length()) {
      saveWifi(ssid, pass);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid.c_str(), pass.c_str());
      uint32_t t0 = millis();
      while (WiFi.status() != WL_CONNECTED && (millis() - t0) < WIFI_CONNECT_WINDOW_MS) { delay(100); }
      if (WiFi.status() == WL_CONNECTED) { stopRescueAP(); Serial.println("OK: WIFI_JOIN"); }
      else { startRescueAP(); Serial.println("ERR: WIFI_JOIN"); }
    } else {
      Serial.println("ERR: WIFI_JOIN");
    }
    return;
  }

  if (line.equalsIgnoreCase("FORGET")) {
    forgetWifi();
    Serial.println("OK: FORGET");
    return;
  }

  if (line.startsWith("/trackname")) {
    // /trackname <index> <name...>
    int sp1 = line.indexOf(' ');
    if (sp1 > 0) {
      int sp2 = line.indexOf(' ', sp1+1);
      if (sp2 > 0) {
        String idxStr = line.substring(sp1+1, sp2);
        String name   = line.substring(sp2+1);
        // TODO: setTrackName(idxStr.toInt(), name);
        Serial.println("OK: /trackname");
        return;
      }
    }
    Serial.println("ERR: /trackname");
    return;
  }

  if (line.startsWith("/activetrack")) {
    // /activetrack <index>
    int sp1 = line.indexOf(' ');
    if (sp1 > 0) {
      String idxStr = line.substring(sp1+1);
      // TODO: selectTrack(idxStr.toInt());
      Serial.println("OK: /activetrack");
      return;
    }
    Serial.println("ERR: /activetrack");
    return;
  }

  if (line.startsWith("/ping"))       { Serial.println("PONG"); return; }
  if (line.startsWith("/reannounce")) { /* TODO: announce */ Serial.println("OK: /reannounce"); return; }

  if (line.startsWith("OTA_URL")) {
    // OTA_URL <http(s)://...bin>
    int sp1 = line.indexOf(' ');
    if (sp1 > 0) {
      String url = line.substring(sp1+1);
      if (WiFi.status() == WL_CONNECTED) { Serial.println("OK: OTA_START"); performHttpUpdate(url); }
      else { Serial.println("ERR: OTA_NO_WIFI"); }
      return;
    }
    Serial.println("ERR: OTA_URL");
    return;
  }

  if (line.equalsIgnoreCase("HIDE_INTRO")) { introEnabled=false; Serial.println("OK: HIDE_INTRO"); return; }
  if (line.equalsIgnoreCase("SHOW_INTRO")) { introEnabled=true;  oledIntroReset(); Serial.println("OK: SHOW_INTRO"); return; }

  Serial.println("ERR: unknown");
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);
  delay(50);

  // OLED init (I2C 0x3C is common; change if yours differs)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // OLED not present; continue without blocking
  } else {
    display.clearDisplay();
    display.display();
  }

  // NVS
  prefs.begin(PREF_NS, false);
  wiredOnly = prefs.getUChar(PREF_WIRED, 1) == 1;

  // Network bring-up by mode
  if (wiredOnly) {
    WiFi.mode(WIFI_OFF);
    stopRescueAP();
    introEnabled = true;       // wired mode => show OLED tips
  } else {
    if (!beginSTAFromPrefs(WIFI_CONNECT_WINDOW_MS)) startRescueAP();
    introEnabled = false;      // Wi-Fi mode => no OLED tips
  }

  // On-device web if radio is on
  if (WiFi.getMode() != WIFI_OFF) beginWeb();

  // OLED intro — show base slide first, then rotate 1↔2 later (wired only)
  oledIntroReset();

  Serial.println("Booted v" FW_VERSION);
}

void loop() {
  // Web server
  if (WiFi.getMode() != WIFI_OFF) server.handleClient();

  // Rescue AP watchdog
  if (!wiredOnly) {
    if (WiFi.status() == WL_CONNECTED && rescueApActive) stopRescueAP();
    if (rescueApActive && (millis() - rescueApStartedAt) > RESCUE_AP_TIMEOUT_MS) stopRescueAP();
  }

  // Serial line reader
  static String rx;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { handleSerialLine(rx); rx = ""; }
    else {
      rx += c;
      if (rx.length() > 512) rx.remove(0); // safety
    }
  }

  // OLED onboarding — wired-only and drawn last
  oledIntroTick();
}
