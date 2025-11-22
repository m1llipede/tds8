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
#include <vector>
#include <ArduinoJson.h>
#include <ctype.h>
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
const char* AP_SSID = "TDS8+Setup";
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

// Heartbeat from M4L
volatile bool m4lConnected = false;     // true while we keep hearing pings
unsigned long connectedUntil = 0;       // millis() deadline to stay "connected"
bool hbOn = false;
unsigned long hbNextToggle = 0;
const unsigned long hbPeriod = 500;   // 500 ms blink


// ---------- Forward declarations ----------
void tcaSelect(uint8_t i);
void drawTrackName(uint8_t screen, const String& name);
void refreshAll();
void showNetworkSetup();               // big, simple 4-line SETUP
void showNetworkSplash(const String& ip);

void handlePing(OSCMessage &msg); 
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
  loadConfig();  
  loadWifiCreds();
  if (wifiSSID.length()) WiFi.begin(wifiSSID.c_str(), wifiPW.c_str());
  else                   WiFi.begin(); // use stored system creds if any

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 6000) {
    delay(250);
  }

//WiFi Connection code
// after the 6s connect attempt:
if (WiFi.status() != WL_CONNECTED) {
  showNetworkSetup();           // draw your OLED setup page

  WiFi.persistent(false);       // avoid NVS churn
  WiFi.mode(WIFI_AP_STA);       // allow AP + STA
  WiFi.disconnect(true, true);  // clear STA creds + stop STA
  WiFi.softAPdisconnect(true);  // ensure no old AP is lingering
  delay(100);

  WiFiManager wm;
  wm.setConfigPortalBlocking(true); // blocking = consistent behavior
  wm.setConfigPortalTimeout(180);   // or 0 for no timeout

  // This ALWAYS starts AP + portal now:
  bool ok = wm.startConfigPortal(AP_SSID, AP_PW);
  if (!ok) {
    // If portal timed out, leave AP visible so you can still reach it
    WiFi.softAP(AP_SSID, AP_PW);
    Serial.printf("AP up: %s (pw %s) at %s\n",
                  AP_SSID, AP_PW, WiFi.softAPIP().toString().c_str());
  }
}


//Uncomment this to reset WiFi pw for testing new environments
/* 
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
  */

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

// Handle UDP (ASCII first, else OSC) — no peekBytes used
int packetSize = Udp.parsePacket();
if (packetSize > 0) {
  const int MAXBUF = 512;
  static uint8_t buf[MAXBUF];
  int n = Udp.read(buf, min(packetSize, MAXBUF - 1));   // read and consume the packet
  if (n < 0) n = 0;
  buf[n] = 0;                                           // NUL-terminate for String use

  bool handledAscii = false;

  // If payload looks like ASCII, try simple commands
  if (n > 0 && isprint(buf[0])) {
    String line((char*)buf);
    line.trim();

// PING <port>
if (line.startsWith("PING")) {
  uint16_t replyPort = Udp.remotePort();
  int sp = line.indexOf(' ');
  if (sp > 0) {
    int p = line.substring(sp + 1).toInt();
    if (p > 0 && p < 65536) replyPort = (uint16_t)p;
  }
  IPAddress rip = Udp.remoteIP();
  Udp.beginPacket(rip, replyPort);
  Udp.print("PONG 1 ");
  Udp.print(replyPort);
  Udp.endPacket();

  // start/refresh the heartbeat window and blinking
  connectedUntil = millis() + 4000UL;
  if (!m4lConnected) {
    m4lConnected = true;
    hbOn = true;                         // show dot immediately
    hbNextToggle = millis() + hbPeriod;  // schedule first blink flip
    drawTrackName(7, trackNames[7]);     // refresh Track 8 now
  }

  handledAscii = true;
}
// REANNOUNCE
else if (line.equalsIgnoreCase("REANNOUNCE")) {
  broadcastIP();
  handledAscii = true;
}



  // If not handled as ASCII, try OSC using the same bytes
  if (!handledAscii) {
    OSCMessage msg;
    for (int i = 0; i < n; ++i) msg.fill(buf[i]);
    if (!msg.hasError()) {
      msg.dispatch("/ping",        handlePing);
      msg.dispatch("/trackname",   handleTrackName);
      msg.dispatch("/activetrack", handleActiveTrack);
      msg.dispatch("/reannounce",  handleReannounceOSC);
    }
  }

  // any UDP activity means the host likely found us → stop beacons
  discoveryActive = false;
}



// Drop the "connected" flag if pings stop
if (m4lConnected && millis() > connectedUntil) {
  m4lConnected = false;
  drawTrackName(7, trackNames[7]);     // remove the dot on Track 8
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

// --- helper: measure text width for a given size
static uint16_t textWidth(const String& s, uint8_t ts) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(ts);
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

// --- helper: join words [0..k-1] with single spaces
static String joinWords(const std::vector<String>& words, int start, int endExclusive) {
  String out;
  for (int i = start; i < endExclusive; ++i) {
    if (i > start) out += ' ';
    out += words[i];
  }
  return out;
}

void drawTrackName(uint8_t screen, const String& name) {
  tcaSelect(screen);
  display.invertDisplay(screen == activeTrack);
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);

  // Border
  display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

// connection dot for Track 8 (index 7) — only when heartbeat is ON
if (screen == 7 && m4lConnected && hbOn) {
  display.fillCircle(SCREEN_WIDTH - 4, 4, 2, SSD1306_WHITE);
}

  // Nothing else if empty
  String raw = name; raw.trim();
  if (raw.length() == 0) { display.display(); return; }

  // Layout constants
  const int MARGIN = 3;                 // gap from border
  const int GAP    = 2;                 // gap between two lines
  const int maxW   = SCREEN_WIDTH - 2 * MARGIN;
  const int CHAR_H = 8;                 // Adafruit default font height @ size=1
  const int CHAR_W = 6;                 // approx width incl spacing @ size=1

  // Normalize spaces
  String s; s.reserve(raw.length());
  bool lastSpace = false;
  for (size_t i = 0; i < raw.length(); ++i) {
    char c = raw[i];
    if (c == ' ' || c == '\t') { if (!lastSpace) { s += ' '; lastSpace = true; } }
    else { s += c; lastSpace = false; }
  }
  while (s.length() && s[0] == ' ') s.remove(0,1);
  while (s.length() && s[s.length()-1] == ' ') s.remove(s.length()-1);

  // Word split
  std::vector<String> words;
  if (s.length()) {
    int start = 0;
    for (int i = 0; i <= (int)s.length(); ++i) {
      if (i == (int)s.length() || s[i] == ' ') {
        words.push_back(s.substring(start, i));
        start = i + 1;
      }
    }
  }

  auto drawSingleLine = [&](const String& line, uint8_t ts) {
    display.setTextSize(ts);
    int w = textWidth(line, ts);
    int lineH = CHAR_H * ts;
    int x = MARGIN + (maxW - w) / 2;
    int y = (SCREEN_HEIGHT - lineH) / 2;
    display.setCursor(x, y);
    display.println(line);
  };

  auto drawTwoLines = [&](const String& l1, const String& l2, uint8_t ts) {
    display.setTextSize(ts);
    int w1 = textWidth(l1, ts);
    int w2 = textWidth(l2, ts);
    int lineH = CHAR_H * ts;
    int totalH = lineH * 2 + GAP;
    int y0 = (SCREEN_HEIGHT - totalH) / 2;

    int x1 = MARGIN + (maxW - w1) / 2;
    display.setCursor(x1, y0);
    display.println(l1);

    int x2 = MARGIN + (maxW - w2) / 2;
    display.setCursor(x2, y0 + lineH + GAP);
    display.println(l2);
  };

  auto trimToWidth = [&](String &line, uint8_t ts) {
    // make sure line fits width; if not, trim to ... (at least 3 chars)
    int maxChars = max(1, maxW / (CHAR_W * ts));
    if ((int)line.length() > maxChars) {
      if (maxChars >= 3) line = line.substring(0, maxChars - 3) + "...";
      else               line = line.substring(0, maxChars);
    }
  };

  // Try largest layout first: size=2
  for (uint8_t ts : { (uint8_t)2, (uint8_t)1 }) {

    // Single word → prefer single line at this size if possible
    if (words.size() <= 1) {
      if (textWidth(s, ts) <= maxW) {
        drawSingleLine(s, ts);
        display.display();
        return;
      }
      // Too long at this size; try two-line hard split
      int maxChars = max(1, maxW / (CHAR_W * ts));
      String l1 = s.substring(0, maxChars);
      String l2 = s.substring(maxChars);
      trimToWidth(l2, ts);
      if (textWidth(l1, ts) <= maxW && textWidth(l2, ts) <= maxW) {
        drawTwoLines(l1, l2, ts);
        display.display();
        return;
      }
      // otherwise fall through and try smaller ts
      continue;
    }

    // Multiple words: search for the best split that fits both lines at this size.
    // Prefer the first split that fits at ts=2; if none, we’ll try ts=1.
    int bestSplit = -1;
    for (int split = 1; split < (int)words.size(); ++split) {
      String l1 = joinWords(words, 0, split);
      String l2 = joinWords(words, split, (int)words.size());
      if (textWidth(l1, ts) <= maxW && textWidth(l2, ts) <= maxW) {
        bestSplit = split; break;
      }
    }

    if (bestSplit >= 0) {
      String l1 = joinWords(words, 0, bestSplit);
      String l2 = joinWords(words, bestSplit, (int)words.size());
      drawTwoLines(l1, l2, ts);
      display.display();
      return;
    }

    // If no word‑based split fits, do a forced two‑line split at char count.
    {
      int maxChars = max(1, maxW / (CHAR_W * ts));
      String all = s;
      String l1 = all.substring(0, maxChars);
      String l2 = all.substring(maxChars);
      trimToWidth(l2, ts);
      if (textWidth(l1, ts) <= maxW && textWidth(l2, ts) <= maxW) {
        drawTwoLines(l1, l2, ts);
        display.display();
        return;
      }
    }
    // else fall through to next (smaller) ts
  }

  // Last resort: size=1, single line trimmed
  {
    uint8_t ts = 1;
    String l = s;
    trimToWidth(l, ts);
    drawSingleLine(l, ts);
    display.display();
  }
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
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    display.setTextWrap(false);  // prevent auto-wrapping/overlap

    const String l0 = "SETUP";
    const String l1 = String("WiFi: ") + AP_SSID;
    const String l2 = String("PW: ")   + AP_PW;
    const String l3 = "Go to: tds8.local";

    auto fit = [&](const String& s)->uint8_t {
      int16_t x1, y1; uint16_t w, h;
      uint8_t ts = 2;                         // try big first
      display.setTextSize(ts);
      display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
      if (w > (SCREEN_WIDTH - 4)) ts = 1;     // drop to 1 if it won’t fit
      return ts;
    };

    uint8_t ts0 = fit(l0);
    uint8_t ts1 = fit(l1);
    uint8_t ts2 = fit(l2);
    uint8_t ts3 = fit(l3);

    const int CHAR_H = 8; // default GFX font height at size=1
    int lineH0 = CHAR_H * ts0;
    int lineH1 = CHAR_H * ts1;
    int lineH2 = CHAR_H * ts2;
    int lineH3 = CHAR_H * ts3;

    int totalH = lineH0 + lineH1 + lineH2 + lineH3 + 6; // 2px gaps
    int y = (SCREEN_HEIGHT - totalH) / 2;

    drawCentered(l0, ts0, y);  y += lineH0 + 2;
    drawCentered(l1, ts1, y);  y += lineH1 + 2;
    drawCentered(l2, ts2, y);  y += lineH2 + 2;
    drawCentered(l3, ts3, y);

    display.display();
  }
}


void showNetworkSplash(const String& ip) {
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

    int16_t x1,y1; uint16_t w,h;
    uint8_t tsTitle = 2;
    display.setTextSize(tsTitle);
    display.getTextBounds("TDS-8 ONLINE", 0, 0, &x1, &y1, &w, &h);
    if (w > (SCREEN_WIDTH - 2)) tsTitle = 1;

    String ipLine = String("IP: ") + ip;
    uint8_t tsIP = 2;
    display.setTextSize(tsIP);
    display.getTextBounds(ipLine, 0, 0, &x1, &y1, &w, &h);
    if (w > (SCREEN_WIDTH - 2)) tsIP = 1;

    drawCentered("TDS-8 ONLINE", tsTitle, 14);
    drawCentered(ipLine, tsIP, 36);
    display.display();
  }
  delay(3000);
  refreshAll();
}


// =====================================================
//                     OSC handlers
// =====================================================

// /ping [replyPort:int] → replies /pong 1 to senderIP:replyPort
// /ping [replyPort:int] → replies /pong 1 to senderIP:replyPort
void handlePing(OSCMessage &msg) {
  uint16_t replyPort = Udp.remotePort();
  if (msg.size() >= 1) {
    replyPort = (uint16_t)((int32_t)msg.getInt(0));   // force 32-bit path
  }

  IPAddress rip = Udp.remoteIP();

  // Build /pong without any ambiguous longs
  OSCMessage m("/pong");

  // --- critical change: avoid templated add(T=long) ---
  // Use explicit OSCData(int32_t) to side-step overload resolution.
  //OSCData d1((int32_t)1);
  //OSCData d2((int32_t)replyPort);
  //m.add(d1);
  //m.add(d2);
  m.add((int64_t)1);
  m.add((int64_t)replyPort);

  Udp.beginPacket(rip, replyPort);
  m.send(Udp);
  Udp.endPacket();

  connectedUntil = millis() + 4000UL;
  if (!m4lConnected) {
    m4lConnected = true;
    drawTrackName(7, trackNames[7]);   // update connection dot on screen 8
  }
}


void handleTrackName(OSCMessage &msg) {
  if (msg.size() < 2) return;
  int idx = msg.getInt(0);
  char buf[64]; msg.getString(1, buf, sizeof(buf));
  if (idx >= 0 && idx < numScreens) {
    String incoming = String(buf);
    if (incoming.length() >= 2 && incoming[0] == '\"' && incoming[incoming.length()-1] == '\"') {
      incoming = incoming.substring(1, incoming.length()-1);
    }
    trackNames[idx] = incoming;
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
  String html = R"TDSEOF(
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
.modal{position:fixed;inset:0;background:rgba(0,0,0,.45);display:flex;align-items:center;justify-content:center;z-index:9999}
.hidden{display:none}
.modal-card{width:min(520px,90vw);background:#fff;border-radius:14px;box-shadow:0 18px 40px rgba(0,0,0,.25);overflow:hidden}
.modal-hdr{padding:14px 16px;background:linear-gradient(135deg,var(--brand),var(--brand2));color:#fff;font-weight:600}
.modal-body{max-height:60vh;overflow:auto;padding:12px 16px}
.modal-ftr{display:flex;justify-content:flex-end;gap:10px;padding:12px 16px;background:#fafbff}
.net{display:flex;align-items:center;justify-content:space-between;border:1px solid #e3e6ef;border-radius:10px;padding:8px 10px;margin:6px 0;cursor:pointer}
.net .ssid{font-weight:600}
.net .meta{color:#777;font-size:12px}
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
        <button class="btn ghost"   id="refresh">Refresh</button>
        <button class="btn ghost"   id="factory">Factory Reset</button>
      </div>
      <div class="kicker" id="announceNote">
        Reannounce will broadcast this device’s IP to your network.
        It may take <b>10–20s</b> for connected apps to update.
      </div>
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

async function post(url, body){
  try{
    const r = await fetch(url,{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify(body||{})
    });
    return await r.text();
  }catch(e){ return 'error'; }
}

function validIPv4(s){
  const m = /^(\d{1,3}\.){3}\d{1,3}$/.test(s);
  if(!m) return false;
  return s.split('.').every(n=>{const x=+n; return x>=0 && x<=255;});
}

// ======= status auto-refresh =======
async function loadStatus(){
  try{
    const r=await fetch('/status'); const j=await r.json();
    ['ip','gw','nm','dns1','dns2'].forEach(k=>{ const el=F(k); if(el) el.textContent=j[k]||'–'; });
  }catch(e){}
}
loadStatus();
setInterval(loadStatus, 2000);

// ======= buttons =======
F('refresh').onclick = ()=> location.reload(true);

F('reannounce').onclick = async ()=>{
  F('msg').textContent='Broadcasting… (clients may update in 10–20s)';
  await post('/reannounce');
  F('msg').textContent='Broadcast sent. Give it 10–20s to propagate.';
  setTimeout(()=>F('msg').textContent='', 3000);
};

F('factory').onclick = async ()=>{
  if(!confirm('Reset static IP to factory defaults? (Wi-Fi password is NOT erased)')) return;
  F('msg').textContent='Resetting… device may pause for 10–20s';
  await post('/reset');
  setTimeout(()=>location.reload(), 12000);
};

// ======= Scan Networks modal =======
const modal = document.getElementById('scanModal');
const results = document.getElementById('scanResults');
document.getElementById('closeScan').onclick = ()=> modal.classList.add('hidden');

F('scan').onclick = async ()=>{
  results.innerHTML = '<div class="note">Scanning…</div>';
  modal.classList.remove('hidden');
  try{
    const r = await fetch('/scan');
    const arr = await r.json();
    if(!Array.isArray(arr) || arr.length===0){
      results.innerHTML = '<div class="note">No networks found. Try again.</div>';
      return;
    }
    results.innerHTML = arr.map(o=>{
      const enc = (o.enc===7 || o.enc==='OPEN') ? 'Open' : 'Secured';
      const rssi = (o.rssi!==undefined) ? `${o.rssi} dBm` : '';
      const ssid = o.ssid || '(hidden)';
      return `<div class="net" data-ssid="${ssid.replace(/"/g,'&quot;')}">
                <div class="ssid">${ssid}</div>
                <div class="meta">${enc} ${rssi}</div>
              </div>`;
    }).join('');
    results.querySelectorAll('.net').forEach(el=>{
      el.onclick = ()=>{
        const ssid = el.dataset.ssid;
        if(ssid && ssid!=='(hidden)') F('ssid').value = ssid;
        modal.classList.add('hidden');
        F('pw').focus();
      };
    });
  }catch(e){
    results.innerHTML = '<div class="note">Scan failed. Ensure device is in AP+STA mode and try again.</div>';
  }
};

// ======= Connect & Save =======
F('connect').onclick = async ()=>{
  const ssid=F('ssid').value.trim(), pw=F('pw').value;
  if(!ssid){ F('msg').textContent='Enter SSID'; return; }
  F('msg').textContent='Connecting… device may pause for 10–20s';
  await post('/wifi',{ssid,pw});
  setTimeout(()=>location.reload(), 12000);
};

// ======= Save static IP (if you wire a button to call this) =======
async function saveStatic(){
  const ip  = (F('ipIn')  ? F('ipIn').value.trim()  : '');
  const gw  = (F('gwIn')  ? F('gwIn').value.trim  () : '');
  const nm  = (F('nmIn')  ? F('nmIn').value.trim()  : '');
  const d1  = (F('dns1In')? F('dns1In').value.trim(): '');
  const d2  = (F('dns2In')? F('dns2In').value.trim(): '');
  const bad = [['IP',ip],['Gateway',gw],['Subnet',nm],['DNS 1',d1],['DNS 2',d2]]
              .filter(([k,v])=>v && !validIPv4(v))
              .map(([k])=>k);
  if(bad.length){ F('msg').textContent='Invalid: '+bad.join(', '); return; }
  F('msg').textContent='Applying… device may pause for 10–20s';
  await post('/save',{ip,gw,nm,dns1:d1,dns2:d2});
  setTimeout(()=>location.reload(), 12000);
}
</script>

<!-- Scan Networks modal -->
<div id="scanModal" class="modal hidden">
  <div class="modal-card">
    <div class="modal-hdr">Available Wi-Fi Networks</div>
    <div class="modal-body" id="scanResults">
      <div class="note">Scanning…</div>
    </div>
    <div class="modal-ftr">
      <button class="btn ghost" id="closeScan">Close</button>
    </div>
  </div>
</div>

</body></html>
)TDSEOF";
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
  WiFi.mode(WIFI_AP_STA);        // ensure AP+STA so scan can run
  delay(200);                    // give RF stack a breath
  int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);

  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["enc"]  = WiFi.encryptionType(i);  // 7=open on some cores
  }
  WiFi.scanDelete();
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}


// POST /save — configure static IP (optional) or DHCP if omitted
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

  // Always start clean
  WiFi.disconnect(true);

  // If user provided an IP, try STATIC first; else pure DHCP
  bool wantStatic = ipS.length() > 0;
  if (wantStatic) WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  else            WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // DHCP

  WiFi.begin();

  // Try to connect for ~8s
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(200);

  // If static was requested but failed, fall back to DHCP automatically
  if (wantStatic && WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // DHCP
    WiFi.begin();
    t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    broadcastIP();
    server.send(200,"application/json","{\"result\":\"ok\"}");
  } else {
    server.send(200,"application/json","{\"result\":\"fail\"}");
  }
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
  currentDNS2.fromString(prefs.getString("dns2", currentDNS2.toString()));
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
