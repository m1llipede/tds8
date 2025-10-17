/**********************************************************************
  TDS-8 (Track Display Strip - 8 channels)
  - 8x SSD1306 (128x64) via TCA9548A I2C multiplexer
  - OSC:
      /trackname  <index> <name>
      /activetrack <index>
  - Wi-Fi:
      1) Quick STA try (stored creds)
      2) Captive-portal via WiFiManager ("TDS8-Setup") if STA fails
  - Web:
      mDNS host: tds8.local
      GET  /status  -> JSON with IP/GW/NM/DNS
      GET  /scan    -> JSON list of SSIDs
      POST /save    -> JSON to set static IP (or leave blank for DHCP)
      POST /reset   -> reset static-IP to defaults
  - UI:
      * On AP (portal) start: show "NETWORK SETUP" instructions on all OLEDs
      * On successful Wi-Fi join: 3s splash "TDS-8 ONLINE / IP: ..."
      * Always draw a border; full invert for active track
**********************************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiManager.h>
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
int activeTrack = -1;  // none highlighted initially

// ---------- Network ----------
Preferences prefs;           // store static-IP values if user saves them
WebServer    server(80);
WiFiUDP      Udp;
const uint16_t oscPort        = 8000;  // OSC listener
const uint16_t ipBroadcastPort= 9000;  // broadcast lease + /ipupdate
const char*   mdnsName        = "tds8"; // http://tds8.local/

// Optional static IP defaults used if user POSTs /save with no values
IPAddress currentIP  (192,168, 1,180);
IPAddress currentGW  (192,168, 1,  1);
IPAddress currentNM  (255,255,255,  0);
IPAddress currentDNS1(  8,  8,  8,  8);
IPAddress currentDNS2(  8,  8,  4,  4);

// ---------- Forward declarations ----------
void tcaSelect(uint8_t i);
void drawTrackName(uint8_t screen, const String& name);
void refreshAll();
void showNetworkSetup();                 // OLED guidance when portal starts
void showNetworkSplash(const String& ip);// OLED "TDS-8 ONLINE / IP: ..."

void handleTrackName(OSCMessage &msg);
void handleActiveTrack(OSCMessage &msg);

void handleRoot();
void handleStatus();
void handleScan();
void handleConfigHTTP(); // POST /save (set static IP or leave blank for DHCP)
void handleReset();      // POST /reset (restore defaults)

void saveConfig();
void loadConfig();

// =====================================================
//                      SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000); // Fast I2C

  // 1) Init all OLEDs first so we can show setup messages if needed
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
      Serial.printf("❌ OLED init failed on channel %d\n", i);
    } else {
      drawTrackName(i, trackNames[i]); // default Track 1..8
    }
  }

  // 2) Try to connect quickly using stored Wi-Fi creds (STA mode)
  WiFi.begin(); // uses creds saved by WiFiManager (if any)
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 6000) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    // 3) Start captive portal; show instructions on the OLEDs
    showNetworkSetup(); // "NETWORK SETUP" / Join TDS8-Setup / tds8.local / 192.168.4.1

    WiFiManager wm;
    // SSID only; open portal (you can set a password with wm.setAPPassword("..."))
    if (!wm.autoConnect("TDS8-Setup")) {
      Serial.println("❌ Captive portal failed — restarting");
      delay(1500);
      ESP.restart();
    }
  }

  // 4) At this point we are connected to Wi-Fi (DHCP by default)
  Serial.println("✅ Wi-Fi connected. IP: " + WiFi.localIP().toString());
  showNetworkSplash(WiFi.localIP().toString());

  // 5) mDNS + HTTP
  if (MDNS.begin(mdnsName)) {
    Serial.println("🌐 mDNS: http://tds8.local");
  } else {
    Serial.println("⚠️ mDNS failed to start");
  }

  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/status",  HTTP_GET,  handleStatus);
  server.on("/scan",    HTTP_GET,  handleScan);
  server.on("/save",    HTTP_POST, handleConfigHTTP); // set static IP (+reconnect)
  server.on("/reset",   HTTP_POST, handleReset);      // restore default static-IP
  server.begin();
  Serial.println("🌐 HTTP server started");

  // 6) OSC + a simple lease broadcast (both ASCII and OSC)
  Udp.begin(oscPort);
  const IPAddress gw = WiFi.gatewayIP();
  const IPAddress bcast(gw[0], gw[1], gw[2], 255);
  const String lease = WiFi.localIP().toString();

  // ASCII broadcast for quick listeners
  Udp.beginPacket(bcast, ipBroadcastPort); Udp.print(lease); Udp.endPacket();

  // OSC broadcast /ipupdate "<ip>"
  OSCMessage m("/ipupdate"); m.add(lease.c_str());
  Udp.beginPacket(bcast, ipBroadcastPort); m.send(Udp); Udp.endPacket();
  Serial.printf("🔊 Broadcast %s → %s:%d\n", lease.c_str(), bcast.toString().c_str(), ipBroadcastPort);
}

// =====================================================
//                       LOOP
// =====================================================
void loop() {
  server.handleClient();

  OSCMessage msg;
  int size = Udp.parsePacket();
  if (size > 0) {
    while (size--) msg.fill(Udp.read());
    if (!msg.hasError()) {
      msg.dispatch("/trackname",   handleTrackName);
      msg.dispatch("/activetrack", handleActiveTrack);
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

  // Full-screen invert if this is the active track
  display.invertDisplay(screen == activeTrack);

  display.clearDisplay();

  // Always draw the outer rectangle (white; it will appear black on inverted screens)
  display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

  // Draw centered text only if non-empty
  String trimmed = name; trimmed.trim();
  if (trimmed.length() > 0) {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(trimmed, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;
    display.setCursor(x, y);
    display.println(trimmed);
  }

  display.display();
  // NOTE: do NOT force invertDisplay(false) here—each screen sets its own state at the top
}

void refreshAll() {
  for (uint8_t i = 0; i < numScreens; i++) {
    drawTrackName(i, trackNames[i]);
  }
}

// =====================================================
//                    OLED helper screens
// =====================================================
void showNetworkSetup() {
  // Shown whenever the captive portal (TDS8-Setup) is about to start
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(6, 8);  display.println("NETWORK SETUP");
    display.setCursor(6, 24); display.println("Join WiFi: TDS8-Setup");
    display.setCursor(6, 36); display.println("Open: tds8.local");
    display.setCursor(6, 48); display.println("or 192.168.4.1");

    display.display();
  }
}

void showNetworkSplash(const String& ip) {
  // Shown for 3s after successful STA connection
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 14); display.println("TDS-8 ONLINE");
    display.setCursor(10, 34); display.print  ("IP: ");
    display.println(ip);

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
    String newName = String(buf);
    // Keep last valid name; empty strings will display as outline-only
    trackNames[idx] = newName;
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

// =====================================================
//                     HTTP endpoints
// =====================================================

// Simple landing page (kept minimal on-device)
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>TDS-8 Network</title>
<style>body{font-family:sans-serif;margin:24px}input{width:100%}</style>
</head><body>
<h2>TDS-8 Network</h2>
<p>Status below reflects current DHCP/static info.</p>
<button onclick="loadStatus()">Refresh</button>
<pre id="out">Loading…</pre>
<script>
async function loadStatus(){
  const r = await fetch('/status'); const j = await r.json();
  out.textContent = JSON.stringify(j,null,2);
}
loadStatus();
</script>
</body></html>)rawliteral";
  server.send(200, "text/html", html);
}

// JSON: { ip, gw, nm, dns1, dns2 } (from current interface)
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

// Scan nearby SSIDs
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

// POST /save   (JSON body) — apply static IP *or* leave blank to revert to DHCP
// Example body:
// {"ip":"192.168.68.180","gw":"192.168.68.1","nm":"255.255.255.0","dns1":"8.8.8.8","dns2":"8.8.4.4"}
// Any missing/empty field → DHCP for that parameter.
void handleConfigHTTP() {
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"result\":\"error\",\"msg\":\"bad json\"}");
    return;
  }

  String ipS   = doc["ip"  ] | "";
  String gwS   = doc["gw"  ] | "";
  String nmS   = doc["nm"  ] | "";
  String d1S   = doc["dns1"] | "";
  String d2S   = doc["dns2"] | "";

  // Save to NVS for persistence
  currentIP.fromString  (ipS.length()? ipS : currentIP.toString());
  currentGW.fromString  (gwS.length()? gwS : currentGW.toString());
  currentNM.fromString  (nmS.length()? nmS : currentNM.toString());
  currentDNS1.fromString(d1S.length()? d1S : currentDNS1.toString());
  currentDNS2.fromString(d2S.length()? d2S : currentDNS2.toString());
  saveConfig();

  // Reconfigure interface
  WiFi.disconnect(true);
  if (ipS.length()) {
    WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  } else {
    // DHCP path: clear static by passing zeroed IPs
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }
  WiFi.begin(); while (WiFi.status() != WL_CONNECTED) delay(200);

  // Broadcast new IP in both ASCII and OSC
  IPAddress gw = WiFi.gatewayIP(); IPAddress bcast(gw[0], gw[1], gw[2], 255);
  String s = WiFi.localIP().toString();
  Udp.beginPacket(bcast, ipBroadcastPort); Udp.print(s); Udp.endPacket();
  OSCMessage m("/ipupdate"); m.add(s.c_str());
  Udp.beginPacket(bcast, ipBroadcastPort); m.send(Udp); Udp.endPacket();

  // Reply
  DynamicJsonDocument out(256);
  out["result"]="ok";
  out["ip"]=WiFi.localIP().toString();
  out["gw"]=WiFi.gatewayIP().toString();
  out["nm"]=WiFi.subnetMask().toString();
  out["dns1"]=WiFi.dnsIP(0).toString();
  out["dns2"]=WiFi.dnsIP(1).toString();
  String body; serializeJson(out, body);
  server.send(200,"application/json",body);
}

// POST /reset → restore static defaults and reconnect with them
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

  // Broadcast
  IPAddress gw = WiFi.gatewayIP(); IPAddress bcast(gw[0], gw[1], gw[2], 255);
  String s = WiFi.localIP().toString();
  Udp.beginPacket(bcast, ipBroadcastPort); Udp.print(s); Udp.endPacket();

  DynamicJsonDocument out(256);
  out["result"]="reset";
  out["ip"]=WiFi.localIP().toString();
  out["gw"]=WiFi.gatewayIP().toString();
  out["nm"]=WiFi.subnetMask().toString();
  out["dns1"]=WiFi.dnsIP(0).toString();
  out["dns2"]=WiFi.dnsIP(1).toString();
  String body; serializeJson(out, body);
  server.send(200,"application/json",body);
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
  currentDNS2.fromString(prefs.getString("dns2", currentDNS2.toString()));
  prefs.end();
}
