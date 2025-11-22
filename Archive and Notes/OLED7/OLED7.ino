#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// OLED and I2C Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_ADDR      0x3C
#define TCA_ADDRESS   0x70
#include <Preferences.h>

Preferences prefs;
IPAddress currentIP, currentGW, currentNM, currentDNS1, currentDNS2;

const int numScreens = 8;

// WiFi Settings (default)
const char* ssid = "Cardinal";
const char* password = "11077381";

// Default static IP config (change as needed)
IPAddress defaultIP(192, 168, 68, 180);
IPAddress defaultGateway(192, 168, 68, 1);
IPAddress defaultSubnet(255, 255, 255, 0);
IPAddress defaultDNS1(8, 8, 8, 8);
IPAddress defaultDNS2(8, 8, 4, 4);

// OSC Settings
WiFiUDP Udp;
const unsigned int localPort = 8000;

// HTTP Server and mDNS
WebServer server(80);
const char* mdnsName = "tds8"; // access at tds8.local

// Shared Display Object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Track Info
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
void handleConfig();

// Select OLED on TCA9548A
void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);
  Wire.endTransmission();
}

// Draw track name centered, with border and optional inversion
void drawTrackName(uint8_t screen, const String& name) {
  tcaSelect(screen);
  display.clearDisplay();

  bool inverted = (screen == activeTrack);
  display.invertDisplay(inverted);
  display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1,
    inverted ? SSD1306_BLACK : SSD1306_WHITE);

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(name, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2;
  display.setCursor(x, y);
  display.println(name);
  display.display();
}

// OSC: /trackname [index] [name]
void handleTrackName(OSCMessage &msg) {
  if (msg.size() < 2) return;
  int index = msg.getInt(0);
  char buf[64];
  msg.getString(1, buf, sizeof(buf));
  if (index >= 0 && index < numScreens) {
    trackNames[index] = String(buf);
    drawTrackName(index, trackNames[index]);
    Serial.printf("✅ Track %d updated to: %s\n", index, buf);
  }
}

// OSC: /activetrack [index]
void handleActiveTrack(OSCMessage &msg) {
  if (msg.size() < 1) return;
  int index = msg.getInt(0);
  if (index >= 0 && index < numScreens) {
    activeTrack = index;
    Serial.printf("🎯 Active Track: %d\n", activeTrack);
    for (int i = 0; i < numScreens; i++) drawTrackName(i, trackNames[i]);
  }
}

// OSC: /ipconfig [ip] [gateway] [subnet] ([dns1] [dns2])
void handleIPConfig(OSCMessage &msg) {
  if (msg.size() < 3) {
    Serial.println("❌ /ipconfig requires IP, gateway, and subnet");
    return;
  }
  char ipBuf[16], gwBuf[16], nmBuf[16], dns1Buf[16], dns2Buf[16];
  msg.getString(0, ipBuf, sizeof(ipBuf));
  msg.getString(1, gwBuf, sizeof(gwBuf));
  msg.getString(2, nmBuf, sizeof(nmBuf));
  IPAddress ip, gw, nm, dns1, dns2;
  ip.fromString(ipBuf);
  gw.fromString(gwBuf);
  nm.fromString(nmBuf);
  if (msg.size() >= 4) msg.getString(3, dns1Buf, sizeof(dns1Buf)); else strcpy(dns1Buf, defaultDNS1.toString().c_str());
  if (msg.size() >= 5) msg.getString(4, dns2Buf, sizeof(dns2Buf)); else strcpy(dns2Buf, defaultDNS2.toString().c_str());
  dns1.fromString(dns1Buf);
  dns2.fromString(dns2Buf);

  WiFi.disconnect(true);
  if (WiFi.config(ip, gw, nm, dns1, dns2)) {
    Serial.printf("📡 Static IP: %s, GW: %s, Mask: %s\n", ipBuf, gwBuf, nmBuf);
  } else {
    Serial.println("❌ Failed to set static IP");
  }
  Serial.print("Reconnecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.println("✅");
  Serial.printf("New IP: %s\n", WiFi.localIP().toString().c_str());
}

// HTTP: root page with IP config form
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>TDS8 Configuration</title>
  <style>
    :root {
      --brand: #005faf;
      --brand-dark: #004080;
      --bg: #f0f2f5;
      --card: #ffffff;
      --text: #333333;
      --muted: #777777;
      --success: #28a745;
      --danger: #dc3545;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      display: flex;
      align-items: center;
      justify-content: center;
      height: 100vh;
    }
    .card {
      background: var(--card);
      border-radius: 8px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
      max-width: 420px;
      width: 100%;
      overflow: hidden;
    }
    .card-header {
      background: linear-gradient(135deg, var(--brand), var(--brand-dark));
      padding: 20px;
      text-align: center;
      color: #fff;
    }
    .card-header h1 {
      font-size: 1.5em;
      font-weight: 400;
    }
    .card-body {
      padding: 20px;
    }
    label {
      display: block;
      margin-top: 15px;
      font-size: 0.9em;
      color: var(--muted);
    }
    input[type="text"] {
      margin-top: 5px;
      width: 100%;
      padding: 10px;
      font-size: 1em;
      border: 1px solid #ccc;
      border-radius: 4px;
    }
    .buttons {
      display: flex;
      justify-content: space-between;
      margin-top: 20px;
    }
    .buttons button {
      flex: 1;
      padding: 12px;
      font-size: 1em;
      border: none;
      border-radius: 4px;
      color: #fff;
      cursor: pointer;
      margin: 0 5px;
    }
    .buttons button#update {
      background: var(--brand);
    }
    .buttons button#reset {
      background: var(--danger);
    }
    .buttons button:disabled {
      opacity: 0.6;
      cursor: not-allowed;
    }
    #progress {
      width: 100%;
      height: 8px;
      margin-top: 20px;
      border-radius: 4px;
      overflow: hidden;
      background: #e0e0e0;
      display: none;
    }
    #progress div {
      height: 100%;
      width: 0%;
      background: var(--success);
      transition: width 0.2s ease;
    }
    #status {
      margin-top: 10px;
      text-align: center;
      font-size: 0.9em;
      color: var(--text);
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="card-header">
      <h1>TDS8 Network Settings</h1>
    </div>
    <div class="card-body">
      <form id="configForm">
        <label for="ip">IP Address</label>
        <input type="text" id="ip" placeholder="192.168.68.180">
        <label for="gw">Gateway</label>
        <input type="text" id="gw" placeholder="192.168.68.1">
        <label for="nm">Subnet Mask</label>
        <input type="text" id="nm" placeholder="255.255.255.0">
        <label for="dns1">DNS 1</label>
        <input type="text" id="dns1" placeholder="8.8.8.8">
        <label for="dns2">DNS 2</label>
        <input type="text" id="dns2" placeholder="8.8.4.4">

        <div class="buttons">
          <button type="submit" id="update">Update</button>
          <button type="button" id="reset">Reset</button>
        </div>

        <div id="progress"><div></div></div>
        <div id="status"></div>
      </form>
    </div>
  </div>

  <script>
    const form   = document.getElementById('configForm');
    const btnUpd = document.getElementById('update');
    const btnRst = document.getElementById('reset');
    const prog   = document.getElementById('progress');
    const bar    = prog.querySelector('div');
    const status = document.getElementById('status');

    function setLoading(on) {
      btnUpd.disabled = on;
      btnRst.disabled = on;
      if (on) {
        prog.style.display = 'block';
        bar.style.width = '0%';
        status.textContent = 'Applying…';
      } else {
        setTimeout(()=> {
          prog.style.display = 'none';
          status.textContent = '';
        }, 1500);
      }
    }

    function postJSON(path, data) {
      setLoading(true);
      return fetch(path, {
        method: 'POST',
        headers: { 'Content-Type':'application/json' },
        body: JSON.stringify(data)
      })
      .then(resp => {
        // simulate gradual fill
        let pct = 0;
        const iv = setInterval(()=>{
          pct = Math.min(100, pct + 20);
          bar.style.width = pct+'%';
          if (pct >= 100) clearInterval(iv);
        }, 200);
        return resp.json();
      })
      .then(js => {
        bar.style.width = '100%';
        status.textContent = js.result==='ok' ? 'Updated!' : 'Reset!';
      })
      .finally(() => setLoading(false));
    }

    form.addEventListener('submit', e => {
      e.preventDefault();
      const data = {
        ip:   document.getElementById('ip').value,
        gw:   document.getElementById('gw').value,
        nm:   document.getElementById('nm').value,
        dns1: document.getElementById('dns1').value,
        dns2: document.getElementById('dns2').value
      };
      postJSON('/config', data);
    });

    btnRst.addEventListener('click', () => {
      postJSON('/reset', {});
    });
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}


// HTTP: form POST handler
void handleConfig() {
  if (server.hasArg("ip") && server.hasArg("gw") && server.hasArg("nm")) {
    char ipBuf[16], gwBuf[16], nmBuf[16], dns1Buf[16], dns2Buf[16];
    strncpy(ipBuf, server.arg("ip").c_str(), sizeof(ipBuf));
    strncpy(gwBuf, server.arg("gw").c_str(), sizeof(gwBuf));
    strncpy(nmBuf, server.arg("nm").c_str(), sizeof(nmBuf));
    if (server.hasArg("dns1")) strncpy(dns1Buf, server.arg("dns1").c_str(), sizeof(dns1Buf)); else strcpy(dns1Buf, defaultDNS1.toString().c_str());
    if (server.hasArg("dns2")) strncpy(dns2Buf, server.arg("dns2").c_str(), sizeof(dns2Buf)); else strcpy(dns2Buf, defaultDNS2.toString().c_str());
    IPAddress ip, gw, nm, dns1, dns2;
    ip.fromString(ipBuf);
    gw.fromString(gwBuf);
    nm.fromString(nmBuf);
    dns1.fromString(dns1Buf);
    dns2.fromString(dns2Buf);

    WiFi.disconnect(true);
    WiFi.config(ip, gw, nm, dns1, dns2);
    WiFi.begin(ssid, password);
    server.send(200, "text/html", "<h2>Network updated. New IP: " + WiFi.localIP().toString() + "</h2>");
    return;
  }
  server.send(400, "text/plain", "Invalid parameters");
}

void loadConfig() {
  prefs.begin("net", false);
  // read or fall back to defaults
  currentIP   = IPAddress();
  currentGW   = IPAddress();
  currentNM   = IPAddress();
  currentDNS1 = IPAddress();
  currentDNS2 = IPAddress();
  String s;
  s = prefs.getString("ip",   defaultIP.toString());   currentIP.fromString(s);
  s = prefs.getString("gw",   defaultGateway .toString()); currentGW.fromString(s);
  s = prefs.getString("nm",   defaultSubnet .toString());  currentNM.fromString(s);
  s = prefs.getString("dns1", defaultDNS1.toString()); editionsDNS1.fromString(s);
  s = prefs.getString("dns2", defaultDNS2.toString()); editionsDNS2.fromString(s);
  prefs.end();
}

void saveConfig(){
  prefs.begin("net", false);
  prefs.putString("ip",   currentIP  .toString());
  prefs.putString("gw",   currentGW  .toString());
  prefs.putString("nm",   currentNM  .toString());
  prefs.putString("dns1", currentDNS1.toString());
  prefs.putString("dns2", currentDNS2.toString());
  prefs.end();
}

void handleConfig(){
  // parse JSON body
  DynamicJsonDocument doc(256);
  deserializeJson(doc, server.arg("plain"));
  String s;

  s = doc["ip"].as<String>();   currentIP.fromString(s);
  s = doc["gw"].as<String>();   currentGW.fromString(s);
  s = doc["nm"].as<String>();   currentNM.fromString(s);
  s = doc["dns1"].as<String>(); currentDNS1.fromString(s);
  s = doc["dns2"].as<String>(); currentDNS2.fromString(s);

  WiFi.disconnect(true);
  WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  WiFi.begin(ssid,password);

  saveConfig();
  server.send(200, "application/json", "{\"result\":\"ok\"}");
}

void handleReset(){
  // restore defaults
  currentIP   = defaultIP;
  currentGW   = defaultGateway;
  currentNM   = defaultSubnet;
  currentDNS1 = defaultDNS1;
  currentDNS2 = defaultDNS2;

  WiFi.disconnect(true);
  WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  WiFi.begin(ssid,password);

  saveConfig();
  server.send(200, "application/json", "{\"result\":\"reset\"}");
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
  loadConfig();

  // Static IP by default
  WiFi.config(defaultIP, defaultGateway, defaultSubnet, defaultDNS1, defaultDNS2);
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.println("\n✅ WiFi Connected! IP: " + WiFi.localIP().toString());

  // mDNS responder
  if (MDNS.begin(mdnsName)) {
    Serial.println("mDNS responder started: " + String(mdnsName) + ".local");
  }

 //MDNS.begin("tds8");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_POST, handleConfig);
  server.on("/reset",  HTTP_POST, handleReset);
  server.begin();

  // OSC listener
  Udp.begin(localPort);
  Serial.printf("🛰️ OSC port %d\n", localPort);

  // HTTP handlers
  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_POST, handleConfig);
  server.begin();
  Serial.println("🌐 HTTP server started");

  // Initialize OLED screens
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR))
      Serial.printf("❌ OLED init failed %d\n", i);
    else
      drawTrackName(i, trackNames[i]);
  }
}

void loop() {
  server.handleClient();  // handle web requests

  OSCMessage msg;
  int size = Udp.parsePacket();
  if (size > 0) {
    while (size--) msg.fill(Udp.read());
    if (!msg.hasError()) {
      msg.dispatch("/trackname", handleTrackName);
      msg.dispatch("/activetrack", handleActiveTrack);
      msg.dispatch("/ipconfig", handleIPConfig);
    }
  }
}
