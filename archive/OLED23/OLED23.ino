/**********************************************************************
  TDS-8 (Track Display Strip - 8 channels)
  - 8x SSD1306 via TCA9548A (manual tcaSelect)
  - OSC: /trackname <idx> <name>, /activetrack <idx>, /ping [replyPort], /reannounce
  - First run w/o Wi-Fi: captive portal "TDS8" (pw: tds8)
  - Web UI: http://tds8.local  (status, scan/join Wi-Fi, reset, reannounce, tracks editor)
  - On connect: brief splash + quick-start screens until Ableton sends names
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
#include <vector>

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
int activeTrack = -1;                     // none selected initially

// ---------- Network / services ----------
Preferences prefs;
WebServer    server(80);
WiFiUDP      Udp;

const uint16_t oscPort         = 8000;    // OSC listener
const uint16_t ipBroadcastPort = 9000;    // broadcast lease + /ipupdate
const char*   mdnsName         = "tds8";  // http://tds8.local/

// Captive-portal AP creds for first-run
const char* AP_SSID = "TDS8";
const char* AP_PW   = "tds8setup";        // "" for open AP

// Optional static IP defaults (kept but DHCP-first behavior)
IPAddress currentIP  (192,168, 1,180);
IPAddress currentGW  (192,168, 1,  1);
IPAddress currentNM  (255,255,255,  0);
IPAddress currentDNS1(  8,  8,  8,  8);
IPAddress currentDNS2(  8,  8,  4,  4);

// Saved user Wi-Fi creds (web-form saves these)
String wifiSSID = "";
String wifiPW   = "";

// Discovery beacons after join (for hosts listening on 9000)
bool        discoveryActive   = false;
unsigned long discoveryUntil  = 0;
unsigned long nextBeaconAt    = 0;
const unsigned long beaconPeriodMs = 5000;
const unsigned long beaconWindowMs = 60000;

// Ableton heartbeat via /ping
volatile bool m4lConnected    = false;   // true while pings arrive
unsigned long connectedUntil  = 0;       // drop after timeout
bool         abletonBannerShown = false; // show once after first trackname
bool quickStartShown = false;  // show Quick-Start once after Wi-Fi connects (no Ableton yet)
// Flash indicator for screen 1 (index 0) on /ping
volatile unsigned long pingFlashUntil = 0;   // time until which the dot should be shown
volatile bool          pingFlashShown = false; // whether we already drew the flash "on" frame
volatile bool wantAbletonBanner = false;
volatile bool wantPingFlash     = false;

// ---- Debug / tracing ----
unsigned long dbgLastBeat = 0;
uint32_t dbgLoopTicks = 0;
uint32_t dbgPackets = 0;
uint32_t dbgPings = 0;
uint32_t dbgDraws = 0;

unsigned long nextFlashDrawAt = 0;
const uint16_t flashRedrawMinMs = 40;  // cap at 25 fps


// ---------- Forward declarations ----------
void tcaSelect(uint8_t i);
void drawTrackName(uint8_t screen, const String& name);
void refreshAll();
void showNetworkSetup();                 // 8 guided screens (no Wi-Fi)
void showNetworkSplash(const String& ip);
void showQuickStartLoop();               // loop steps until track names arrive
void showAbletonConnectedAll(uint16_t ms = 1000);

void handlePing(OSCMessage &msg);
void handleTrackName(OSCMessage &msg);
void handleActiveTrack(OSCMessage &msg);
void handleReannounceOSC(OSCMessage &msg);
void handleTracksReset();  // POST /tracks/reset → sets Track 1..8, saves, refreshes

void handleRoot();
void handleStatus();
void handleScan();
void handleSave();
void handleReset();
void handleReannounceHTTP();
void handleWifiSave();
void handleForgetWifi(); // POST /forget

// NEW: Tracks editor endpoints + persistence
void handleTracksGet();   // GET  /tracks → {names:[...]}
void handleTracksPost();  // POST /tracks {"names":[...]} → saves + refreshes
void saveTrackNames();
void loadTrackNames();

void saveConfig();
void loadConfig();
void saveWifiCreds();
void loadWifiCreds();
void broadcastIP();
void forgetWifi();        // Serial "FORGET" or internal helpers

// helpers
static uint16_t textWidth(const String& s, uint8_t ts);
static String joinWords(const std::vector<String>& words, int start, int endExclusive);
void drawCentered(const String& s, uint8_t textSize, int y);
void drawLines(uint8_t ts, int y0, const char* l1=nullptr, const char* l2=nullptr, const char* l3=nullptr);

// =====================================================
//                      SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000); // Fast I2C
  Wire.setTimeOut(15);  // ms; prevents infinite wait if bus glitches

  // Init all OLEDs
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
      Serial.printf("❌ OLED init failed on channel %d\n", i);
    } else {
      display.clearDisplay();
      display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
      display.display();
    }
  }

  // Load config + Wi-Fi creds + persisted track names
  loadConfig();
  loadWifiCreds();
  loadTrackNames();  // ← RESTORES names saved via web UI (if any)
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Try STA join with creds saved in NVS first, then system creds
  if (wifiSSID.length()) WiFi.begin(wifiSSID.c_str(), wifiPW.c_str());
  else                   WiFi.begin(); // use stored system creds if any

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) delay(250);

  // If not connected -> show Wi-Fi setup pages and force config portal
if (WiFi.status() != WL_CONNECTED) {
  // Only show setup helper screens if we really need user action
  bool haveSaved = wifiSSID.length() > 0;

  // Don’t erase credentials here. Keep station mode live and retry.
  WiFi.persistent(true);                 // keep creds persistent across boots
  WiFi.mode(haveSaved ? WIFI_STA : WIFI_AP_STA);
  WiFi.disconnect(false /* wifioff */, false /* keep creds */);
  WiFi.softAPdisconnect(true);

  if (haveSaved) {
    // We have saved creds—try a longer join before bothering the user.
    Serial.println("⏳ Retrying STA join with saved creds…");
    WiFi.begin(wifiSSID.c_str(), wifiPW.c_str());

    unsigned long t1 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t1 < 20000UL) { // 20s
      delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
      // Stay in STA; don’t erase creds; just show a gentle tip screen once.
      showNetworkSetup(); // optional: comment out if you don’t want any hint
      Serial.println("⚠️ Still not connected; keeping STA and skipping portal.");
    }
  } else {
    // No saved creds -> bring up the config portal
    showNetworkSetup();

    WiFiManager wm;
    wm.setConfigPortalBlocking(true);
    wm.setConfigPortalTimeout(180);
    bool ok = wm.startConfigPortal(AP_SSID, AP_PW);
    if (!ok) {
      WiFi.softAP(AP_SSID, AP_PW);
      IPAddress apip = WiFi.softAPIP();
      Serial.printf("⚠️ Portal timed out; AP up: %s pw:%s IP:%s\n",
                    AP_SSID, AP_PW, apip.toString().c_str());
    }
  }
}


  // Connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ Wi-Fi connected. IP: " + WiFi.localIP().toString());
    showNetworkSplash(WiFi.localIP().toString());
  } else {
    Serial.println("⚠️ Wi-Fi NOT connected; staying in AP/setup mode.");
  }

  // mDNS + HTTP
  if (MDNS.begin(mdnsName)) Serial.println("🌐 mDNS: http://tds8.local");
  else                      Serial.println("⚠️ mDNS failed");

  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/status",     HTTP_GET,  handleStatus);
  server.on("/scan",       HTTP_GET,  handleScan);
  server.on("/save",       HTTP_POST, handleSave);
  server.on("/reset",      HTTP_POST, handleReset);
  server.on("/reannounce", HTTP_POST, handleReannounceHTTP);
  server.on("/wifi",       HTTP_POST, handleWifiSave);
  server.on("/forget",     HTTP_POST, handleForgetWifi);
  server.on("/tracks/reset", HTTP_POST, handleTracksReset);

  // NEW: Tracks editor
  server.on("/tracks",     HTTP_GET,  handleTracksGet);
  server.on("/tracks",     HTTP_POST, handleTracksPost);

  server.begin();
  Serial.println("🌐 HTTP server started");

  // OSC + initial IP broadcast
  Udp.begin(oscPort); 
  Serial.printf("UDP begin on %u %s\n", oscPort, Udp.remotePort()==0 ? "" : "(remote?)");
  Serial.printf("🎧 OSC listening on UDP %u\n", oscPort);

  broadcastIP();

  // Start discovery beacons for the first minute
  discoveryActive  = true;
  discoveryUntil   = millis() + beaconWindowMs;
  nextBeaconAt     = millis() + beaconPeriodMs;

  // If Wi-Fi is up but Ableton hasn’t sent anything yet, show quick-start once
  if (WiFi.status() == WL_CONNECTED) {
    showQuickStartLoop();
  }
}

// =====================================================
//                       LOOP
// =====================================================
void loop() {

      dbgLoopTicks++;
    unsigned long now = millis();
    if (now - dbgLastBeat >= 1000) { // 1s heartbeat
      Serial.printf("[HB] t=%lu ticks=%u pkts=%u pings=%u draws=%u heap=%u wifi=%d\n",
                    now, dbgLoopTicks, dbgPackets, dbgPings, dbgDraws,
                    ESP.getFreeHeap(), (int)WiFi.status());
      dbgLoopTicks = dbgPackets = dbgPings = dbgDraws = 0;
      dbgLastBeat = now;
    }

  server.handleClient();

  // Show the Ableton banner once, outside the UDP handler
  if (wantAbletonBanner) {
    wantAbletonBanner = false;
    showAbletonConnectedAll();   // this has a delay; now it won’t stall UDP
    refreshAll();
  }

  // Flash dot timing for Screen 1
static bool flashOn = false;
if (wantPingFlash) {
  wantPingFlash = false;
  flashOn = true;
  pingFlashUntil = millis() + 150;
}

if (flashOn) {
  if (millis() >= nextFlashDrawAt) {
    drawTrackName(0, trackNames[0]);   // dot drawn inside drawTrackName()
    nextFlashDrawAt = millis() + flashRedrawMinMs;
  }
  if (millis() > pingFlashUntil) {
    flashOn = false;
    drawTrackName(0, trackNames[0]);   // redraw without dot
  }
}


  // …your existing OSC receive/dispatch, discovery, timeouts…
  OSCMessage msg;
  int size;
  while ((size = Udp.parsePacket()) > 0) {
    dbgPackets++;
    while (size--) msg.fill(Udp.read());
    if (!msg.hasError()) {
      msg.dispatch("/ping",        handlePing);
      msg.dispatch("/trackname",   handleTrackName);
      msg.dispatch("/activetrack", handleActiveTrack);
      msg.dispatch("/reannounce",  handleReannounceOSC);
    }
    msg.empty();                   // ✅ free internal buffer promptly
    discoveryActive = false;
    yield();                       // ✅ give Wi-Fi time between packets
  }

  // Drop "connected" dot if pings stop
  if (m4lConnected && millis() > connectedUntil) {
    m4lConnected = false;
    drawTrackName(7, trackNames[7]);                  // remove the dot on Track 8
  }

  // Discovery beacons
  now = millis();
  if (discoveryActive) {
    if (now >= discoveryUntil) {
      discoveryActive = false;
    } else if (now >= nextBeaconAt) {
      broadcastIP();
      nextBeaconAt += beaconPeriodMs;
    }
  }

  // Show Quick-Start ONCE when Wi-Fi is connected but Ableton hasn’t spoken yet
  if (!quickStartShown && WiFi.status() == WL_CONNECTED && !m4lConnected && !abletonBannerShown) {
    showQuickStartLoop();
    quickStartShown = true;
  }
}

// =====================================================
//                 OLED & TCA9548A helpers
// =====================================================
void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);
  uint8_t err = Wire.endTransmission(true);
  if (err) {
    Serial.printf("I2C ERR on TCA select ch=%u code=%u\n", i, err);
  }
}

// measure width at given text size
static uint16_t textWidth(const String& s, uint8_t ts) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(ts);
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

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

  // Nothing else if empty
  String raw = name; raw.trim();

  // Layout constants
  const int MARGIN = 3, GAP = 2, maxW = SCREEN_WIDTH - 2 * MARGIN;
  const int CHAR_H = 8, CHAR_W = 6;

  auto textWidth = [&](const String& s, uint8_t ts)->uint16_t{
    int16_t x1,y1; uint16_t w,h; display.setTextSize(ts);
    display.getTextBounds(s,0,0,&x1,&y1,&w,&h); return w;
  };
  auto drawSingleLine = [&](const String& line, uint8_t ts){
    display.setTextSize(ts);
    int w = textWidth(line, ts);
    int lineH = CHAR_H * ts;
    int x = MARGIN + (maxW - w) / 2;
    int y = (SCREEN_HEIGHT - lineH) / 2;
    display.setCursor(x, y);
    display.println(line);
  };
  auto drawTwoLines = [&](const String& l1, const String& l2, uint8_t ts){
    display.setTextSize(ts);
    int w1 = textWidth(l1, ts), w2 = textWidth(l2, ts);
    int lineH = CHAR_H * ts, totalH = lineH * 2 + GAP;
    int y0 = (SCREEN_HEIGHT - totalH) / 2;
    int x1 = MARGIN + (maxW - w1) / 2;
    display.setCursor(x1, y0); display.println(l1);
    int x2 = MARGIN + (maxW - w2) / 2;
    display.setCursor(x2, y0 + lineH + GAP); display.println(l2);
  };
  auto trimToWidth = [&](String &line, uint8_t ts){
    int maxChars = max(1, maxW / (CHAR_W * ts));
    if ((int)line.length() > maxChars) {
      if (maxChars >= 3) line = line.substring(0, maxChars - 3) + "...";
      else               line = line.substring(0, maxChars);
    }
  };

  if (raw.length()) {
    // normalize spaces + split into words
    String s; s.reserve(raw.length()); bool lastSpace = false;
    for (size_t i=0;i<raw.length();++i){ char c=raw[i];
      if (c==' '||c=='\t'){ if(!lastSpace){ s+=' '; lastSpace=true; } }
      else { s+=c; lastSpace=false; } }
    while (s.length() && s[0]==' ') s.remove(0,1);
    while (s.length() && s[s.length()-1]==' ') s.remove(s.length()-1);

    std::vector<String> words;
    if (s.length()){
      int start=0; for (int i=0;i<=(int)s.length();++i){
        if (i==(int)s.length() || s[i]==' '){ words.push_back(s.substring(start,i)); start=i+1; }
      }
    }

    bool drawn=false;
    for (uint8_t ts : { (uint8_t)2, (uint8_t)1 }) {
      if (words.size() <= 1) {
        if (textWidth(s, ts) <= maxW) { drawSingleLine(s, ts); drawn=true; break; }
        int maxChars = max(1, maxW/(CHAR_W*ts));
        String l1 = s.substring(0,maxChars), l2 = s.substring(maxChars);
        trimToWidth(l2, ts);
        if (textWidth(l1, ts) <= maxW && textWidth(l2, ts) <= maxW) {
          drawTwoLines(l1,l2,ts); drawn=true; break;
        }
        continue;
      }
      int bestSplit=-1;
      for (int split=1; split<(int)words.size(); ++split){
        String l1, l2;
        for (int i=0;i<split;++i){ if(i) l1+=' '; l1+=words[i]; }
        for (int i=split;i<(int)words.size();++i){ if(i>split) l2+=' '; l2+=words[i]; }
        if (textWidth(l1, ts)<=maxW && textWidth(l2, ts)<=maxW){ drawTwoLines(l1,l2,ts); drawn=true; break; }
      }
      if (drawn) break;
      int maxChars = max(1, maxW/(CHAR_W*ts));
      String all=s, l1=all.substring(0,maxChars), l2=all.substring(maxChars);
      trimToWidth(l2, ts);
      if (textWidth(l1, ts)<=maxW && textWidth(l2, ts)<=maxW){ drawTwoLines(l1,l2,ts); drawn=true; break; }
    }
    if (!drawn){ uint8_t ts=1; String l=s; trimToWidth(l, ts); drawSingleLine(l, ts); }
  }

  // ← draw flash dot LAST (so it isn't cleared) and ALWAYS execute
  if (screen == 0 && millis() <= pingFlashUntil) {
    display.fillCircle(4, SCREEN_HEIGHT - 4, 2, SSD1306_WHITE);
  }

  unsigned long t0 = micros();
  display.display();
  unsigned long dt = micros() - t0;
  dbgDraws++;
  if (dt > 5000) { // >5ms is interesting on 400kHz I2C
    Serial.printf("drawTrackName screen=%u took %lu us\n", screen, dt);
  }
  return; // <-- keep your delay(0) after this line if you want, but the time log should be before it.

  display.display();
  delay(0);     // ✅ yield so Wi-Fi/UDP keeps breathing
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

// Draw up to 3 centered lines at text size ts
void drawLines(uint8_t ts, int y0, const char* l1, const char* l2, const char* l3) {
  const int CHAR_H = 8;
  int lineH = CHAR_H * ts;
  int y = y0;
  if (l1){ drawCentered(String(l1), ts, y); y += lineH + 2; }
  if (l2){ drawCentered(String(l2), ts, y); y += lineH + 2; }
  if (l3){ drawCentered(String(l3), ts, y); }
}

// =====================================================
//                OLED helper screens
// =====================================================

void showNetworkSetup() {
  // Draw border + show 5 setup tips; remaining screens show border only
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    display.setTextWrap(false);

    switch (i) {
      case 0: // INITIAL SETUP (centered, 2 lines)
        drawLines(2, 15, "INITIAL", "SETUP", ""); break;
      case 1: // Connect to 'TDS8' WiFi
        drawLines(2, 15, "Connect to", "'TDS8' WiFi", ""); break;
      case 2: // WiFi Password: tds8setup
        drawLines(2, 15, "Password:", "tds8setup", ""); break;
      case 3: // Open Browser tds8.local
        drawLines(2, 15, "WiFi Mgr", "Config WiFi" , ""); break;
      case 4: // Open Browser tds8.local
        drawLines(1, 20, "msftconnecttest.com", "/redirect", ""); break;
      case 5: // Open Browser tds8.local
        drawLines(2, 15, "Connect to", "your WiFi", ""); break;
      case 6: // Open Browser tds8.local
        drawLines(2, 15, "Wait ~20s", "to Connect", ""); break;
      case 7: // Wait ~20s to Connect
        drawLines(2, 15, "Browser:", "tds8.local", ""); break;

      default: break;
    }
    display.display();
  }
}

// Only draw the "TDS8 ONLINE" splash on the 8th display (index 7)
// Uses tight spacing at size=2; falls back to size=1 if needed.
void showNetworkSplash(const String& ip) {
  const uint8_t i = 7;  // screen 8 (0-based)
  tcaSelect(i);
  display.invertDisplay(false);
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

  // Try size-2 title with custom gap between "TDS8" and "ONLINE"
  uint8_t ts = 2;
  display.setTextSize(ts);

  int16_t x1, y1; uint16_t w1, h1, w2, h2;
  display.getTextBounds("TDS8",   0, 0, &x1, &y1, &w1, &h1);
  display.getTextBounds("ONLINE", 0, 0, &x1, &y1, &w2, &h2);

  const int maxW = SCREEN_WIDTH - 6;  // leave a little margin inside border
  bool drewTitle = false;
  for (int gap = 2; gap >= 0; --gap) {   // try gap=2,1,0
    int total = (int)w1 + gap + (int)w2;
    if (total <= maxW) {
      int x = (SCREEN_WIDTH - total) / 2;
      int y = 10;
      display.setCursor(x, y);                display.print("TDS8");
      display.setCursor(x + w1 + gap, y);     display.print("ONLINE");
      drewTitle = true;
      break;
    }
  }

  if (!drewTitle) {
    ts = 1;
    display.setTextSize(ts);
    drawCentered("TDS8 ONLINE", ts, 10);
  }

  // IP line (always size 1)
  drawCentered("IP: " + ip, 1, 36);
  display.display();
  delay(1500);
}

void showAbletonConnectedAll(uint16_t ms) {  // default 0
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    drawLines(2, 15, "Ableton", "Connected", "");
    display.display();
    delay(0);  // yield each screen
  }
  if (ms) delay(ms);
  refreshAll();
}


// While we have Wi-Fi but not yet any /trackname: loop simple quick-start pages once
void showQuickStartLoop() {
  for (uint8_t i = 0; i < 7; ++i) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

    switch (i % 8) {
      case 0: drawLines(2, 15, "Launch", "Ableton", ""); break;
      case 1: drawLines(2, 2,  "Drag", "TDS8.amxd", "to a track"); break;
      case 2: drawLines(2, 15, "Browser:", "tds8.local", ""); break;
      case 3: drawLines(2, 4,  "Hit", "Refresh", "Displays"); break;
      case 4: drawLines(2, 4,  "", "", ""); break;
      case 5: drawLines(2, 4,  "", "", ""); break;
      case 6: drawLines(2, 4,  "", "", ""); break;
      case 7: drawLines(2, 4,  "", "", ""); break;

    }
    display.display();
  }
  delay(500);
}

// =====================================================
//                     OSC handlers
// =====================================================

void handlePing(OSCMessage &msg) {
  dbgPings++;
  uint16_t replyPort = Udp.remotePort();   // where the ping came from
  if (msg.size() >= 1) {
    if (msg.isInt(0))       replyPort = (uint16_t)((int32_t)msg.getInt(0));
    else if (msg.isFloat(0)) replyPort = (uint16_t)msg.getFloat(0);
  }
  IPAddress rip = Udp.remoteIP();

  // Minimal reply
  OSCMessage m("/pong"); m.add((int32_t)1);
  Udp.beginPacket(rip, replyPort); m.send(Udp); Udp.endPacket();

  // Only set flags/timers — no drawing here
  connectedUntil = millis() + 4000UL;
  m4lConnected   = true;

  pingFlashUntil = millis() + 150;
  wantPingFlash  = true;

  Serial.printf("PING from %s:%u (reply->%u)\n",
                Udp.remoteIP().toString().c_str(),
                Udp.remotePort(),
                replyPort);
}


void handleTrackName(OSCMessage &msg) {
  if (msg.size() < 2) return;
  int idx = msg.getInt(0);
  char buf[64]; msg.getString(1, buf, sizeof(buf));
  if (idx >= 0 && idx < numScreens) {
    if (!abletonBannerShown) {
    abletonBannerShown = true;
    wantAbletonBanner = true;   // ✅ schedule the banner instead of drawing now
    }

    String incoming = String(buf);
    // Remove quotes if Max sent "Name"
    if (incoming.length() >= 2 &&
        incoming[0] == '\"' &&
        incoming[incoming.length()-1] == '\"') {
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
  // Adds a Tracks Editor card with 8 fields + Save button
  String html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
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
.value,.input{padding:10px 12px;border:1px solid #e3e6ef;border-radius:10px;background:#fafbff}
.input{background:#fff;width:100%}
.actions{display:flex;gap:10px;margin-top:12px;flex-wrap:wrap}
.btn{appearance:none;border:0;border-radius:10px;padding:10px 14px;cursor:pointer}
.btn.primary{background:var(--brand);color:#fff}
.btn.ghost{background:#fff;border:1px solid #e3e6ef}
.note{margin-top:8px;color:var(--muted)}
/* Tracks editor */
.trackGrid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
.trackCard{border:1px solid #e3e6ef;border-radius:12px;padding:10px;background:#fff}
.trackCard h3{margin:0 0 6px 0;font-size:13px;color:#555}
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
        <button class="btn ghost"   id="forget">Forget Wi-Fi</button>
      </div>
      <div class="note" id="msg"></div>
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
      <div class="note">After connecting, the page may pause ~10–20s while the device switches networks.</div>
    </div>
  </div>

  <!-- NEW: Tracks Editor -->
  <div class="card">
    <div class="hdr"><h1>Tracks Editor</h1></div>
    <div class="body">
      <p class="note">Type names for each display and click "Save Names" to push them live. These are also saved to the device so they persist after reboot (until Ableton overwrites via OSC).</p>
      <div id="trackGrid" class="trackGrid"></div>
      <div class="actions">
        <button class="btn primary" id="saveNames">Save Names</button>
        <button class="btn ghost" id="reloadNames">Reload From Device</button>
        <button class="btn ghost" id="resetNames">Reset Names to Defaults</button> 
      </div>
      <div class="note" id="tracksMsg"></div>
    </div>
  </div>

</div>

<!-- Scan modal -->
<div id="scanModal" class="modal hidden" style="position:fixed;inset:0;background:rgba(0,0,0,.45);display:none;align-items:center;justify-content:center;z-index:9999">
  <div class="modal-card" style="width:min(520px,90vw);background:#fff;border-radius:14px;box-shadow:0 18px 40px rgba(0,0,0,.25);overflow:hidden">
    <div class="modal-hdr" style="padding:14px 16px;background:linear-gradient(135deg,var(--brand),var(--brand2));color:#fff;font-weight:600">Available Wi-Fi Networks</div>
    <div class="modal-body" id="scanResults" style="max-height:60vh;overflow:auto;padding:12px 16px"><div class="note">Scanning…</div></div>
    <div class="modal-ftr" style="display:flex;justify-content:flex-end;gap:10px;padding:12px 16px;background:#fafbff"><button class="btn ghost" id="closeScan">Close</button></div>
  </div>
</div>

<script>
const F=id=>document.getElementById(id);

// ----- Status -----
async function loadStatus(){
  try{
    const r=await fetch('/status'); const j=await r.json();
    ['ip','gw','nm','dns1','dns2'].forEach(k=>{ const el=F(k); if(el) el.textContent=j[k]||'–'; });
  }catch(e){}
}
loadStatus(); setInterval(loadStatus, 2000);

async function post(url, body){
  try{
    const r = await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{})});
    return await r.text();
  }catch(e){ return 'error'; }
}

F('refresh').onclick = ()=> location.reload(true);

F('reannounce').onclick = async ()=>{
  F('msg').textContent='Broadcasting… (clients may update in ~10–20s)';
  await post('/reannounce'); setTimeout(()=>F('msg').textContent='',3000);
};

F('resetNames').onclick = async ()=>{
  tmsg.textContent = 'Resetting...';
  const res = await post('/tracks/reset', {});
  if(res.includes('"ok"')){
    await loadTracks();                 // re-pull from device (now no-cached)
    tmsg.textContent = 'Names reset.';
  } else {
    tmsg.textContent = 'Reset failed.';
  }
  setTimeout(()=>tmsg.textContent='', 2000);
};

const modal = document.getElementById('scanModal');
const results = document.getElementById('scanResults');
document.getElementById('closeScan').onclick = ()=> { modal.style.display='none'; };

F('scan').onclick = async ()=>{
  results.innerHTML = '<div class="note">Scanning…</div>';
  modal.style.display='flex';
  try{
    const r = await fetch('/scan'); const arr = await r.json();
    if(!Array.isArray(arr) || arr.length===0){ results.innerHTML='<div class="note">No networks found.</div>'; return; }
    results.innerHTML = arr.map(o=>{
      const enc = (o.enc===7 || o.enc==='OPEN') ? 'Open' : 'Secured';
      const rssi = (o.rssi!==undefined) ? `${o.rssi} dBm` : '';
      const ssid = o.ssid || '(hidden)';
      return `<div class="net" data-ssid="${ssid.replace(/"/g,'&quot;')}" style="display:flex;align-items:center;justify-content:space-between;border:1px solid #e3e6ef;border-radius:10px;padding:8px 10px;margin:6px 0;cursor:pointer">
                <div class="ssid" style="font-weight:600">${ssid}</div>
                <div class="meta" style="color:#777;font-size:12px">${enc} ${rssi}</div>
              </div>`;
    }).join('');
    results.querySelectorAll('.net').forEach(el=>{
      el.onclick = ()=>{
        const ssid = el.dataset.ssid;
        if(ssid && ssid!=='(hidden)') F('ssid').value = ssid;
        modal.style.display='none'; F('pw').focus();
      };
    });
  }catch(e){ results.innerHTML='<div class="note">Scan failed.</div>'; }
};

F('connect').onclick = async ()=>{
  const ssid=F('ssid').value.trim(), pw=F('pw').value;
  if(!ssid){ F('msg').textContent='Enter SSID'; return; }
  F('msg').textContent='Connecting… device may pause for 10–20s';
  await post('/wifi',{ssid,pw}); setTimeout(()=>location.reload(), 12000);
};

F('forget').onclick = async ()=>{ F('msg').textContent = 'Forgetting Wi-Fi… device will reboot into setup'; await post('/forget'); };

// ----- Tracks Editor -----
const tgrid = document.getElementById('trackGrid');
const tmsg  = document.getElementById('tracksMsg');
let names = Array(8).fill('');

function renderTracks(){
  tgrid.innerHTML = '';
  for(let i=0;i<8;i++){
    const card = document.createElement('div'); card.className='trackCard';
    const h = document.createElement('h3'); h.textContent = `Track ${i+1}`;
    const inp = document.createElement('input'); inp.className='input'; inp.id='name'+i; inp.value = names[i] || `Track ${i+1}`;
    card.appendChild(h); card.appendChild(inp);
    tgrid.appendChild(card);
  }
}

async function loadTracks(){
  try{
    const r = await fetch('/tracks', { cache: 'no-store' });
    const j = await r.json();
    names = Array.isArray(j.names) && j.names.length===8 ? j.names : names;
  }catch(e){}
  renderTracks();
}
F('reloadNames').onclick = loadTracks;

F('saveNames').onclick = async ()=>{
  const out = [];
  for(let i=0;i<8;i++){ out.push(document.getElementById('name'+i).value); }
  tmsg.textContent = 'Saving...';
  const res = await post('/tracks',{names:out});
  if(res.includes('"ok"')){
    tmsg.textContent = 'Names saved & pushed to displays.';
    names = out.slice();
  }else{
    tmsg.textContent = 'Save failed.';
  }
  setTimeout(()=>tmsg.textContent='',2500);
};

loadTracks();
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
  WiFi.mode(WIFI_AP_STA);
  delay(200);
  int n = WiFi.scanNetworks(false, true);

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
  bool wantStatic = ipS.length() > 0;
  if (wantStatic) WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  else            WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

  WiFi.begin();
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(200);

  if (wantStatic && WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
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
    discoveryActive = true;
    discoveryUntil  = millis() + beaconWindowMs;
    nextBeaconAt    = millis() + beaconPeriodMs;
    server.send(200,"application/json","{\"result\":\"ok\"}");
  } else {
    server.send(200,"application/json","{\"result\":\"fail\"}");
  }
}

// ---------- NEW: Tracks endpoints + persistence ----------
void handleTracksGet() {
  DynamicJsonDocument doc(1024);
  JsonArray namesArr = doc.createNestedArray("names");
  for (int i=0;i<numScreens;i++) namesArr.add(trackNames[i]);
  String out; serializeJson(doc, out);

  // prevent browser caching
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.send(200, "application/json", out);
}


void handleTracksPost() {
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"bad json\"}");
    return;
  }
  if (!doc.containsKey("names") || !doc["names"].is<JsonArray>()) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"missing names[]\"}");
    return;
  }
  JsonArray arr = doc["names"].as<JsonArray>();
  for (uint8_t i=0;i<numScreens && i<arr.size();i++){
    trackNames[i] = String((const char*)arr[i]);
  }
  saveTrackNames(); // persist to NVS
  refreshAll();     // live push
  server.send(200,"application/json","{\"result\":\"ok\"}");
}

void handleTracksReset() {
  // reset to "Track 1".."Track 8"
  for (int i = 0; i < numScreens; i++) {
    trackNames[i] = "Track " + String(i + 1);
  }
  saveTrackNames();   // persist to NVS
  refreshAll();       // push to OLEDs
  server.send(200, "application/json", "{\"result\":\"ok\"}");
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

// Persist track names individually to avoid big blobs
void saveTrackNames(){
  prefs.begin("tracks", false);
  for (int i=0;i<numScreens;i++){
    String key = "name" + String(i);
    prefs.putString(key.c_str(), trackNames[i]);
  }
  prefs.end();
}
void loadTrackNames(){
  prefs.begin("tracks", true);
  for (int i=0;i<numScreens;i++){
    String key = "name" + String(i);
    String v = prefs.getString(key.c_str(), trackNames[i]);
    trackNames[i] = v;
  }
  prefs.end();
}

// =====================================================
//           Broadcast current IP (ASCII + OSC)
// =====================================================
void broadcastIP() {
  if (WiFi.status() != WL_CONNECTED) return;  // ← guard

  IPAddress gw = WiFi.gatewayIP();
  IPAddress bcast(gw[0], gw[1], gw[2], 255);
  String s = WiFi.localIP().toString();

  Udp.beginPacket(bcast, ipBroadcastPort); Udp.print(s); Udp.endPacket();

  OSCMessage m("/ipupdate");
  m.add(s.c_str()); // string is fine per your rule
  Udp.beginPacket(bcast, ipBroadcastPort); m.send(Udp); Udp.endPacket();
  delay(0);   
  Serial.printf("🔊 Announce: %s → %s:%d\n", s.c_str(), bcast.toString().c_str(), ipBroadcastPort);
}

void handleForgetWifi() {
  // Clear SDK creds (STA), turn Wi-Fi off
  WiFi.disconnect(true /*wifioff*/, true /*erasePersistent*/);
  WiFi.mode(WIFI_OFF);
  delay(200);

  // Clear WiFiManager-stored settings too (ESP32 SDK/NVS)
  {
    WiFiManager wm;
    wm.resetSettings();
  }

  // Clear our own saved SSID/PW
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  // Respond then reboot to come up in AP/setup mode
  server.send(200, "application/json", "{\"result\":\"ok\"}");
  delay(500);
  ESP.restart();
}

void forgetWifi() {
  Serial.println("Forgetting Wi-Fi credentials and rebooting...");

  // Erase SDK/NVS Wi-Fi creds and turn Wi-Fi off
  WiFi.disconnect(true /* wifioff */, true /* erasePersistent */);
  WiFi.mode(WIFI_OFF);
  delay(200);

  // Also clear WiFiManager (if any)
  {
    WiFiManager wm;
    wm.resetSettings();
  }

  // Clear our saved SSID/PW in Preferences
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  delay(200);
  ESP.restart();
}
