#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiManager.h>       // Captive-portal manager
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// OLED and I2C Settings
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET     -1
#define I2C_ADDR        0x3C
#define TCA_ADDRESS     0x70

// NVS for static-IP config
Preferences prefs;
IPAddress currentIP, currentGW, currentNM, currentDNS1, currentDNS2;

// Default static-IP
IPAddress defaultIP      (192,168, 1,180);
IPAddress defaultGateway (192,168, 1,  1);
IPAddress defaultSubnet  (255,255,255,  0);
IPAddress defaultDNS1    (  8,  8,  8,  8);
IPAddress defaultDNS2    (  8,  8,  4,  4);

// UDP broadcast port
WiFiUDP    Udp;
const uint16_t ipBroadcastPort = 9000;

// HTTP & mDNS
WebServer server(80);
const char* mdnsName = "tds8"; // http://tds8.local/

// OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Track screens
const int numScreens = 8;
String trackNames[numScreens] = {
  "Track 1","Track 2","Track 3","Track 4",
  "Track 5","Track 6","Track 7","Track 8"
};
int activeTrack = -1;

// Forward declarations
void tcaSelect(uint8_t i);
void drawTrackName(uint8_t screen, const String& name);
void handleTrackName(OSCMessage &msg);
void handleActiveTrack(OSCMessage &msg);
void handleIPConfig(OSCMessage &msg);

void handleRoot();
void handleConfigHTTP();
void handleReset();
void handleStatus();
void handleScan();

void loadConfig();
void saveConfig();

void setup() {
  Serial.begin(115200);
  Wire.begin();  
  Wire.setClock(400000);

  // 1) Captive-portal AP + DHCP join
  WiFiManager wm;
  if (!wm.autoConnect("TDS8-Setup")) {
    Serial.println("❌ Captive portal failed — restarting");
    delay(2000);
    ESP.restart();
  }
  // Now on your LAN via DHCP
  Serial.println("✅ DHCP IP: " + WiFi.localIP().toString());

  // 2) Prepare UDP and endpoints
  Udp.begin(8000);  // OSC listener port (you can also use 0 to pick any)
  const IPAddress gw    = WiFi.gatewayIP();
  const IPAddress bcast = IPAddress(gw[0], gw[1], gw[2], 255);
  const String    lease = WiFi.localIP().toString();

  // 3a) Send plain ASCII UDP lease
  Udp.beginPacket(bcast, 9000);
  Udp.print(lease);
  Udp.endPacket();

  // 3b) Send OSC‐framed lease
  {
    OSCMessage m("/ipupdate");
    m.add(lease.c_str());
    Udp.beginPacket(bcast, 9000);
    m.send(Udp);
    Udp.endPacket();
    Serial.println("🎺 OSC /ipupdate sent");
  }

  Serial.printf("🔊 Broadcast %s → %s:9000\n",
                lease.c_str(), bcast.toString().c_str());

  // 4) mDNS, HTTP, OLED init, etc...
  if (MDNS.begin("tds8")) {
    Serial.println("🌐 mDNS: http://tds8.local");
  }
  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/status", HTTP_GET,  handleStatus);
  server.begin();
  Serial.println("🌐 HTTP server started");

  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
      Serial.printf("❌ OLED init failed %d\n", i);
    } else {
      drawTrackName(i, trackNames[i]);
    }
  }
}



void loop() {
  server.handleClient();

  OSCMessage msg;
  int size = Udp.parsePacket();
  if (size > 0) {
    while (size--) msg.fill(Udp.read());
    if (!msg.hasError()) {
      msg.dispatch("/trackname",  handleTrackName);
      msg.dispatch("/activetrack", handleActiveTrack);
      msg.dispatch("/ipconfig",    handleIPConfig);
    }
  }
}

//────────────────────────────────────────────────────────
// Load & save static-IP from NVS
//────────────────────────────────────────────────────────
void loadConfig() {
  prefs.begin("net", true);
  currentIP  .fromString(prefs.getString("ip",   defaultIP     .toString()));
  currentGW  .fromString(prefs.getString("gw",   defaultGateway.toString()));
  currentNM  .fromString(prefs.getString("nm",   defaultSubnet .toString()));
  currentDNS1.fromString(prefs.getString("dns1", defaultDNS1   .toString()));
  currentDNS2.fromString(prefs.getString("dns2", defaultDNS2   .toString()));
  prefs.end();
}

void saveConfig() {
  prefs.begin("net", false);
  prefs.putString("ip",   currentIP  .toString());
  prefs.putString("gw",   currentGW  .toString());
  prefs.putString("nm",   currentNM  .toString());
  prefs.putString("dns1", currentDNS1.toString());
  prefs.putString("dns2", currentDNS2.toString());
  prefs.end();
}

//────────────────────────────────────────────────────────
// OLED & TCA9548A helpers
//────────────────────────────────────────────────────────
void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);
  Wire.endTransmission();
}

void drawTrackName(uint8_t screen, const String& name) {
  tcaSelect(screen);
  display.clearDisplay();
  bool inv = (screen == activeTrack);
  display.invertDisplay(inv);
  display.drawRect(0, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1,
                   inv ? SSD1306_BLACK : SSD1306_WHITE);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds(name,0,0,&x1,&y1,&w,&h);
  display.setCursor((SCREEN_WIDTH-w)/2,(SCREEN_HEIGHT-h)/2);
  display.println(name);
  display.display();
}

//────────────────────────────────────────────────────────
// OSC handlers (unchanged)
//────────────────────────────────────────────────────────
void handleTrackName(OSCMessage &msg) {
  if (msg.size()<2) return;
  int idx = msg.getInt(0);
  char buf[64]; msg.getString(1, buf, sizeof(buf));
  if (idx>=0 && idx<numScreens) {
    trackNames[idx] = String(buf);
    drawTrackName(idx, trackNames[idx]);
  }
}

void handleActiveTrack(OSCMessage &msg) {
  if (msg.size()<1) return;
  int idx = msg.getInt(0);
  if (idx<0||idx>=numScreens||idx==activeTrack) return;
  int old = activeTrack;
  activeTrack = idx;
  if (old>=0) drawTrackName(old, trackNames[old]);
  drawTrackName(activeTrack, trackNames[activeTrack]);
}

void handleIPConfig(OSCMessage &msg) {
  // your existing OSC /ipconfig logic here...
}

//────────────────────────────────────────────────────────
// HTTP endpoints
//────────────────────────────────────────────────────────

void handleScan() {
  int n = WiFi.scanNetworks();
  DynamicJsonDocument doc(512);
  auto arr = doc.to<JsonArray>();
  for(int i=0;i<n;i++){
    auto o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  String out; serializeJson(doc,out);
  server.send(200,"application/json",out);
}

//────────────────────────────────────────────────────────
// HTTP: return current DHCP info as JSON
//────────────────────────────────────────────────────────
void handleStatus() {
  DynamicJsonDocument doc(256);
  doc["ip"]   = WiFi.localIP().toString();
  doc["gw"]   = WiFi.gatewayIP().toString();
  doc["nm"]   = WiFi.subnetMask().toString();
  // ESP32 supports two DNS slots
  IPAddress d1 = WiFi.dnsIP(0);
  IPAddress d2 = WiFi.dnsIP(1);
  doc["dns1"] = d1.toString();
  doc["dns2"] = d2.toString();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

//────────────────────────────────────────────────────────
// HTTP: serve the simplified Network-Config page
//────────────────────────────────────────────────────────
void handleRoot() {
  // Build and send a single HTML page that fetches /status on load
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en"><head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>TDS8 Network Config</title>
  <style>
    :root {
      --brand: #ff8800; --brand-dark: #800080;
      --bg: #fff; --card: #fff; --text: #333;
    }
    * { box-sizing:border-box; margin:0; padding:0; }
    body {
      font-family:sans-serif; background:var(--bg);
      display:flex; align-items:center; justify-content:center;
      height:100vh;
    }
    .card {
      background:var(--card); border-radius:8px;
      box-shadow:0 4px 12px rgba(0,0,0,0.1);
      max-width:400px; width:100%; overflow:hidden;
    }
    .hdr {
      background:linear-gradient(135deg,var(--brand),var(--brand-dark));
      padding:20px; text-align:center; color:#fff;
    }
    .hdr h1 { font-size:1.4em; font-weight:400; margin:0; }
    .body { padding:20px; }
    label { display:block; margin-top:12px; color:var(--text); }
    input {
      margin-top:4px; width:100%; padding:8px;
      font-size:1em; border:1px solid #ccc; border-radius:4px;
    }
    #refresh {
      margin-top:20px; width:100%; padding:12px;
      font-size:1em; background:var(--brand);
      border:none; border-radius:4px; color:#fff;
      cursor:pointer;
    }
    #status {
      margin-top:12px; text-align:center; color:var(--text);
    }
  </style>
</head><body>
  <div class="card">
    <div class="hdr"><h1>TDS8 Network Config</h1></div>
    <div class="body">
      <label>IP Address<input type="text" id="ip" readonly></label>
      <label>Gateway   <input type="text" id="gw" readonly></label>
      <label>Subnet    <input type="text" id="nm" readonly></label>
      <label>DNS 1     <input type="text" id="dns1" readonly></label>
      <label>DNS 2     <input type="text" id="dns2" readonly></label>
      <button id="refresh">Refresh</button>
      <div id="status"></div>
    </div>
  </div>

  <script>
    const fields = ['ip','gw','nm','dns1','dns2'];
    const refreshBtn = document.getElementById('refresh');
    const statusDiv  = document.getElementById('status');

    async function loadStatus(){
      statusDiv.textContent = 'Loading…';
      try {
        const res = await fetch('/status');
        const js  = await res.json();
        fields.forEach(f => document.getElementById(f).value = js[f]);
        statusDiv.textContent = 'Done';
      } catch (e) {
        statusDiv.textContent = 'Error';
      }
      setTimeout(()=> statusDiv.textContent = '', 1500);
    }

    refreshBtn.addEventListener('click', loadStatus);
    // Auto-load on page open
    window.addEventListener('DOMContentLoaded', loadStatus);
  </script>
</body></html>
)rawliteral";

  server.send(200, "text/html", html);
}



void handleConfigHTTP() {
  // parse JSON, reconnect as before…
  DynamicJsonDocument doc(256);
  if(deserializeJson(doc,server.arg("plain"))){
    server.send(400,"application/json","{\"result\":\"error\"}");
    return;
  }
  currentIP .fromString(doc["ip"].as<const char*>());
  currentGW .fromString(doc["gw"].as<const char*>());
  currentNM .fromString(doc["nm"].as<const char*>());
  currentDNS1.fromString(doc["dns1"].as<const char*>());
  currentDNS2.fromString(doc["dns2"].as<const char*>());

  // reboot Wi-Fi to apply new static IP
  WiFi.disconnect(true);
  WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  WiFi.begin();  // auto-connect uses SSID/pass from captive-portal
  while(WiFi.status()!=WL_CONNECTED) delay(200);

  // persist
  saveConfig();

  // broadcast ASCII IP
  {
    auto g = WiFi.gatewayIP();
    IPAddress b(g[0],g[1],g[2],255);
    String s = WiFi.localIP().toString();
    Udp.beginPacket(b, ipBroadcastPort);
    Udp.print(s);
    Udp.endPacket();
  }

  // reply full JSON
  String r = String("{\"result\":\"ok\",")
    + "\"ip\":\""+currentIP.toString()+"\","
    + "\"gw\":\""+currentGW.toString()+"\","
    + "\"nm\":\""+currentNM.toString()+"\","
    + "\"dns1\":\""+currentDNS1.toString()+"\","
    + "\"dns2\":\""+currentDNS2.toString()+"\"}";
  server.send(200,"application/json",r);
}

void handleReset() {
  currentIP   = defaultIP;
  currentGW   = defaultGateway;
  currentNM   = defaultSubnet;
  currentDNS1 = defaultDNS1;
  currentDNS2 = defaultDNS2;
  saveConfig();

  WiFi.disconnect(true);
  WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  WiFi.begin();
  while(WiFi.status()!=WL_CONNECTED) delay(200);

  // broadcast
  {
    auto g = WiFi.gatewayIP();
    IPAddress b(g[0],g[1],g[2],255);
    String s = currentIP.toString();
    Udp.beginPacket(b, ipBroadcastPort);
    Udp.print(s);
    Udp.endPacket();
  }

  String r = String("{\"result\":\"reset\",")
    + "\"ip\":\""+currentIP.toString()+"\","
    + "\"gw\":\""+currentGW.toString()+"\","
    + "\"nm\":\""+currentNM.toString()+"\","
    + "\"dns1\":\""+currentDNS1.toString()+"\","
    + "\"dns2\":\""+currentDNS2.toString()+"\"}";
  server.send(200,"application/json",r);
}
