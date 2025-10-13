/**************************************************************************
  TDS-8  (ESP32-C3)  —  OLED27_OTA_Wired.ino
  • Defaults to wired-only (USB serial) control
  • Optional Wi-Fi join/forget via serial commands
  • Local device web UI (served only when Wi-Fi is up)
  • OTA by URL (OTA_URL <http(s)://…bin>)
**************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Update.h>

// ----------------------------- Version -----------------------------------
static const char* FW_VERSION = "1.25";

// ----------------------------- Globals -----------------------------------
Preferences prefs;

// runtime flags (single definitions; do not duplicate)
bool wiredOnly           = true;   // default: wired (USB) only
bool abletonBannerShown  = false;
bool quickStartShown     = false;

// Wi-Fi state
String storedSSID;
String storedPASS;
bool   wifiConfigured    = false;
bool   wifiConnected     = false;

// Web server (only active when Wi-Fi is connected)
WebServer server(80);

// ----------------------- Forward declarations ----------------------------
void handleSerialLine(const String& lineRaw);
void ensureWifiStarted();
void startAPorConnect();
void startHttpServer();
void stopHttpServer();
void doOtaFromUrl(const String& url);

// ------------------------------ Setup ------------------------------------
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("TDS-8 booting…");

  // Load persisted comm settings
  prefs.begin("comm", false);
  wiredOnly = prefs.getBool("wiredOnly", true);
  storedSSID = prefs.getString("ssid", "");
  storedPASS = prefs.getString("pass", "");
  wifiConfigured = storedSSID.length() > 0;
  prefs.end();

  Serial.printf("VERSION: %s\r\n", FW_VERSION);
  Serial.printf("WIRED_ONLY: %s\r\n", wiredOnly ? "true" : "false");
  if (wifiConfigured) {
    Serial.printf("WIFI_CFG: ssid=\"%s\"\r\n", storedSSID.c_str());
  } else {
    Serial.println("WIFI_CFG: none");
  }

  // We default to wired-only. If user had explicitly enabled Wi-Fi earlier
  // and wants it on, they can send WIFI_ON from the bridge or serial.
  if (!wiredOnly && wifiConfigured) {
    ensureWifiStarted();
  }
}

// ------------------------------- Loop ------------------------------------
static String lineBuf;

void loop() {
  // Basic serial line reader
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (lineBuf.length()) {
        handleSerialLine(lineBuf);
        lineBuf = "";
      }
    } else {
      lineBuf += c;
      if (lineBuf.length() > 1024) lineBuf.remove(0, lineBuf.length() - 1024);
    }
  }

  // Serve HTTP only if Wi-Fi live
  if (wifiConnected) {
    server.handleClient();
  }
}

// ------------------------- Serial Command Parser -------------------------
static String trimLower(const String& s) {
  String t = s;
  t.trim();
  t.toLowerCase();
  return t;
}

static String unquote(const String& s) {
  String t = s;
  t.trim();
  if (t.length() >= 2 && ((t[0] == '\"' && t[t.length()-1] == '\"') ||
                          (t[0] == '\'' && t[t.length()-1] == '\''))) {
    return t.substring(1, t.length()-1);
  }
  return t;
}

void handleSerialLine(const String& lineRaw) {
  String line = lineRaw;
  line.trim();
  if (!line.length()) return;

  // simple tokenization
  String head = line;
  int sp = line.indexOf(' ');
  if (sp >= 0) head = line.substring(0, sp);

  String lowHead = head; lowHead.toLowerCase();

  // --- Diagnostics / app commands ---
  if (lowHead == "version") {
    Serial.printf("VERSION: %s\r\n", FW_VERSION);
    return;
  }
  if (line.equalsIgnoreCase("/ping") || lowHead == "ping") {
    Serial.println("PONG");
    return;
  }
  if (line.startsWith("/trackname ")) {
    String name = line.substring(String("/trackname ").length());
    name.trim();
    Serial.printf("ACK trackname \"%s\"\r\n", name.c_str());
    // TODO: route to your track logic
    return;
  }
  if (line.startsWith("/activetrack ")) {
    String idx = line.substring(String("/activetrack ").length());
    idx.trim();
    Serial.printf("ACK activetrack %s\r\n", idx.c_str());
    // TODO: route to your track logic
    return;
  }
  if (line.equalsIgnoreCase("/reannounce")) {
    Serial.println("ANNOUNCE: TDS-8 online");
    return;
  }

  // --- Wired/Wi-Fi control ---
  if (lowHead == "wired_only" || lowHead == "wiredonly") {
    String arg = line.substring(head.length()); arg.trim();
    arg.toLowerCase();
    bool on = (arg == "true" || arg == "1" || arg == "yes");
    wiredOnly = on;
    prefs.begin("comm", false);
    prefs.putBool("wiredOnly", wiredOnly);
    prefs.end();
    Serial.printf("OK WIRED_ONLY %s\r\n", wiredOnly ? "true" : "false");
    if (!wiredOnly && wifiConfigured) {
      ensureWifiStarted();
    } else if (wiredOnly) {
      // If turning wiredOnly back on, stop Wi-Fi/server
      if (wifiConnected) {
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        wifiConnected = false;
        stopHttpServer();
        Serial.println("WIFI: off");
      }
    }
    return;
  }

  if (lowHead == "wifi_on" || lowHead == "wifi") {
    if (wiredOnly) {
      Serial.println("ERR: wired-only; set WIRED_ONLY false to allow Wi-Fi");
      return;
    }
    if (!wifiConfigured) {
      Serial.println("ERR: no stored Wi-Fi creds; use WIFI_JOIN \"ssid\" \"pass\"");
      return;
    }
    ensureWifiStarted();
    return;
  }

  if (lowHead == "wifi_join") {
    if (sp < 0) { Serial.println("ERR: WIFI_JOIN \"ssid\" \"password\""); return; }
    // parse two quoted args
    String rest = line.substring(sp + 1); rest.trim();
    // find first quoted
    int q1 = rest.indexOf('\"');
    int q1e = rest.indexOf('\"', q1 + 1);
    if (q1 < 0 || q1e < 0) { Serial.println("ERR: WIFI_JOIN needs quoted SSID and PASS"); return; }
    String ssid = rest.substring(q1, q1e + 1);
    String rest2 = rest.substring(q1e + 1); rest2.trim();
    int q2 = rest2.indexOf('\"');
    int q2e = rest2.indexOf('\"', q2 + 1);
    if (q2 < 0 || q2e < 0) { Serial.println("ERR: WIFI_JOIN needs quoted SSID and PASS"); return; }
    String pass = rest2.substring(q2, q2e + 1);

    storedSSID = unquote(ssid);
    storedPASS = unquote(pass);
    wifiConfigured = storedSSID.length() > 0;

    prefs.begin("comm", false);
    prefs.putString("ssid", storedSSID);
    prefs.putString("pass", storedPASS);
    prefs.putBool("wiredOnly", wiredOnly);
    prefs.end();
    Serial.printf("WIFI_CFG: saved ssid=\"%s\"\r\n", storedSSID.c_str());

    if (!wiredOnly) ensureWifiStarted();
    return;
  }

  if (lowHead == "forget" || lowHead == "wifi_forget") {
    prefs.begin("comm", false);
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();
    storedSSID = "";
    storedPASS = "";
    wifiConfigured = false;
    Serial.println("WIFI_CFG: cleared");
    // Disconnect if connected
    if (wifiConnected) {
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      wifiConnected = false;
      stopHttpServer();
      Serial.println("WIFI: off");
    }
    return;
  }

  // --- OTA by URL ---
  if (lowHead == "ota_url") {
    String url = line.substring(head.length()); url.trim();
    if (!url.length()) { Serial.println("ERR: OTA_URL <http(s)://…bin>"); return; }
    if (wiredOnly) { Serial.println("ERR: wired-only; set WIRED_ONLY false and enable Wi-Fi"); return; }
    if (!wifiConfigured) { Serial.println("ERR: no Wi-Fi configured; WIFI_JOIN first"); return; }
    ensureWifiStarted();
    doOtaFromUrl(url);
    return;
  }

  Serial.println("ERR: unknown");
}

// -------------------------- Wi-Fi / Web Server ---------------------------
void ensureWifiStarted() {
  if (wifiConnected) return;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(storedSSID.c_str(), storedPASS.c_str());
  Serial.printf("WIFI: connecting to \"%s\" …\r\n", storedSSID.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("WIFI: connected %s  ip=%s\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    startHttpServer();
  } else {
    wifiConnected = false;
    Serial.println("WIFI: failed");
  }
}

void startHttpServer() {
  // Root page — mirror your existing handleRoot() HTML exactly.
  // Replace the placeholder HTML below with your current device page markup
  // for a 1:1 match. The desktop bridge will host the exact same HTML.
  server.on("/", HTTP_GET, []() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html");
    // BEGIN: paste your exact handleRoot() HTML (between the quotes of R"rawliteral(... )rawliteral")
    server.sendContent(R"rawliteral(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TDS-8</title>
<style>
/* Paste your exact CSS from firmware here for a pixel-perfect match */
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;margin:0;padding:24px;background:#0b0f14;color:#e6edf3}
.card{background:#121820;border:1px solid #223;border-radius:12px;padding:16px;max-width:720px}
input,button{background:#0e141b;color:#e6edf3;border:1px solid #263241;border-radius:8px;padding:10px 12px}
.row{display:grid;grid-template-columns:1fr 1fr;gap:8px}
</style>
</head>
<body>
<h1>TDS-8</h1>
<div class="card">
  <h3>Communication</h3>
  <div class="row">
    <button onclick="wiredOnly(true)">Wired Only: ON</button>
    <button onclick="wiredOnly(false)">Wired Only: OFF</button>
  </div>
  <div class="row" style="margin-top:8px">
    <button onclick="wifiOn()">Wi-Fi ON</button>
    <button onclick="forget()">Forget Wi-Fi</button>
  </div>
  <div style="margin-top:8px">
    <div class="row">
      <input id="ssid" placeholder="SSID">
      <input id="pwd" placeholder="Password" type="password">
    </div>
    <button style="margin-top:8px" onclick="join()">Join</button>
  </div>
</div>
<script>
async function api(cmd){ try{ const r=await fetch('/api?cmd='+encodeURIComponent(cmd)); console.log(await r.text()); }catch(e){ console.log(e) } }
function wiredOnly(on){ api('WIRED_ONLY '+(on?'true':'false')) }
function wifiOn(){ api('WIFI_ON') }
function forget(){ api('FORGET') }
function join(){ const s=document.getElementById('ssid').value; const p=document.getElementById('pwd').value; api('WIFI_JOIN "'+s.replace(/"/g,'\\"')+'" "'+p.replace(/"/g,'\\"')+'"') }
</script>
</body>
</html>
)rawliteral");
    // END: your page
  });

  // super-simple GET API that proxies one-liner commands (kept for parity)
  server.on("/api", HTTP_GET, []() {
    String cmd = server.hasArg("cmd") ? server.arg("cmd") : "";
    if (!cmd.length()) { server.send(400, "text/plain", "ERR: missing cmd"); return; }
    // Route like serial to reuse parser
    handleSerialLine(cmd);
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("HTTP: server started");
}

void stopHttpServer() {
  server.stop();
}

// ------------------------------- OTA -------------------------------------
void doOtaFromUrl(const String& url) {
  Serial.printf("OTA: fetching %s\r\n", url.c_str());

  HTTPClient http;
  WiFiClient *streamClient = nullptr;
  if (url.startsWith("https:")) {
    // For production: use proper cert pinning or validate root CA.
    // Here we allow insecure for convenience on lab builds.
    WiFiClientSecure *s = new WiFiClientSecure();
    s->setInsecure();
    streamClient = s;
  } else {
    streamClient = new WiFiClient();
  }

  if (!http.begin(*streamClient, url)) {
    Serial.println("OTA: http begin failed");
    delete streamClient;
    return;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("OTA: HTTP %d\r\n", code);
    http.end(); delete streamClient; return;
  }

  int len = http.getSize();
  if (len <= 0) {
    Serial.println("OTA: invalid size");
    http.end(); delete streamClient; return;
  }

  WiFiClient& in = http.getStream();
  if (!Update.begin(len)) {
    Serial.printf("OTA: Update.begin failed err=%d\r\n", Update.getError());
    http.end(); delete streamClient; return;
  }

  size_t written = 0;
  uint8_t buf[4096];
  while (http.connected() && (len > 0 || len == -1)) {
    size_t avail = in.available();
    if (avail) {
      int c = in.readBytes(buf, (avail > sizeof(buf)) ? sizeof(buf) : avail);
      if (c > 0) {
        if (Update.write(buf, c) != (size_t)c) {
          Serial.printf("OTA: write failed err=%d\r\n", Update.getError());
          Update.abort(); http.end(); delete streamClient; return;
        }
        written += c;
        if (len > 0) len -= c;
      }
    }
    delay(1);
  }

  if (!Update.end()) {
    Serial.printf("OTA: end failed err=%d\r\n", Update.getError());
  } else if (Update.isFinished()) {
    Serial.printf("OTA: success, bytes=%u. Rebooting…\r\n", (unsigned)written);
    http.end(); delete streamClient;
    delay(200);
    ESP.restart();
    return;
  } else {
    Serial.println("OTA: not finished");
  }

  http.end();
  delete streamClient;
}
