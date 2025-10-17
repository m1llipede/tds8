#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>  // for DynamicJsonDocument
#include "BluetoothSerial.h"
BluetoothSerial BT; 

// OLED and I2C Settings
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET     -1
#define I2C_ADDR        0x3C
#define TCA_ADDRESS     0x70

Preferences prefs;
Preferences wifiPrefs;
IPAddress currentIP, currentGW, currentNM, currentDNS1, currentDNS2;

const int numScreens = 8;

// WiFi Settings (default)
const char* ssid     = "Cardinal";
const char* password = "11077381";

// Default static IP config
IPAddress defaultIP       (192,168, 68,180);
IPAddress defaultGateway  (192,168, 68,  1);
IPAddress defaultSubnet   (255,255,255,  0);
IPAddress defaultDNS1     (  8,  8,  8,  8);
IPAddress defaultDNS2     (  8,  8,  4,  4);

// OSC + UDP broadcast Settings
WiFiUDP    Udp;
const unsigned int localPort        = 8000;  // OSC port
const unsigned int ipBroadcastPort  = 9000;  // raw UDP port

// HTTP Server and mDNS
WebServer  server(80);
const char* mdnsName = "tds8"; // http://tds8.local/

// Shared Display Object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Track Info
String trackNames[numScreens] = {
  "Track 1","Track 2","Track 3","Track 4",
  "Track 5","Track 6","Track 7","Track 8"
};
int activeTrack = -1;

// forward declarations
void   tcaSelect(uint8_t i);
void   drawTrackName(uint8_t screen, const String& name);
void   handleTrackName(OSCMessage &msg);
void   handleActiveTrack(OSCMessage &msg);
void   handleIPConfig(OSCMessage &msg);
void   handleRoot();
void   handleConfigHTTP();
void   handleReset();
void   loadConfig();
void   saveConfig();


void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  loadConfig();

  if (!WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2)) {
    Serial.println("⚠️ WiFi.config failed");
  }
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected! IP: " + WiFi.localIP().toString());

  if (MDNS.begin(mdnsName)) {
    Serial.println("mDNS responder: " + String(mdnsName) + ".local");
  }

  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/config", HTTP_POST, handleConfigHTTP);
  server.on("/reset",  HTTP_POST, handleReset);
  server.on("/status", HTTP_GET,  handleStatus );
  server.on("/scan",   HTTP_GET,  handleScan   );

  server.begin();
  Serial.println("🌐 HTTP server started");

  Udp.begin(localPort);
  Serial.printf("🛰️ OSC port %d\n", localPort);

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
// OLED & TCA9548A
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
  bool inverted = (screen == activeTrack);
  display.invertDisplay(inverted);
  display.drawRect(0,0,SCREEN_WIDTH-1,SCREEN_HEIGHT-1,
                   inverted?SSD1306_BLACK:SSD1306_WHITE);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds(name,0,0,&x1,&y1,&w,&h);
  display.setCursor((SCREEN_WIDTH-w)/2,(SCREEN_HEIGHT-h)/2);
  display.println(name);
  display.display();
}

//────────────────────────────────────────────────────────
// OSC handlers
//────────────────────────────────────────────────────────
void handleTrackName(OSCMessage &msg) {
  if (msg.size()<2) return;
  int idx = msg.getInt(0);
  char buf[64];
  msg.getString(1, buf, sizeof(buf));
  if (idx>=0 && idx<numScreens) {
    trackNames[idx] = String(buf);
    drawTrackName(idx, trackNames[idx]);
    Serial.printf("✅ Track %d → %s\n", idx, buf);
  }
}

void handleActiveTrack(OSCMessage &msg) {
  static int prev = -1;
  if (msg.size()<1) return;
  int idx = msg.getInt(0);
  if (idx<0||idx>=numScreens||idx==activeTrack) return;
  Serial.printf("🎯 Active Track: %d\n", idx);
  int old = activeTrack;
  activeTrack = idx;
  if (old>=0&&old<numScreens) drawTrackName(old, trackNames[old]);
  drawTrackName(activeTrack, trackNames[activeTrack]);
}

void handleIPConfig(OSCMessage &msg) {
  // (your existing OSC /ipconfig code)
}

// Send back current network config as JSON
void handleStatus() {
  DynamicJsonDocument doc(256);
  doc["ip"]   = currentIP.toString();
  doc["gw"]   = currentGW.toString();
  doc["nm"]   = currentNM.toString();
  doc["dns1"] = currentDNS1.toString();
  doc["dns2"] = currentDNS2.toString();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// Scan for Wi-Fi networks and return a JSON array [{ssid:,rssi:},…]
void handleScan() {
  int n = WiFi.scanNetworks();
  DynamicJsonDocument doc(512);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject obj = arr.createNestedObject();
    obj["ssid"] = WiFi.SSID(i);
    obj["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();  // free resources
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

//────────────────────────────────────────────────────────
// HTTP handlers + HTML form
//────────────────────────────────────────────────────────
void handleRoot(){
  String ip  = currentIP  .toString();
  String gw  = currentGW  .toString();
  String nm  = currentNM  .toString();
  String d1  = currentDNS1.toString();
  String d2  = currentDNS2.toString();

  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en"><head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>TDS8 Network Config</title>
  <style>
    :root {
      --brand: #ff8800; --brand-dark: #800080;
      --bg: #ffffff; --card: #fff; --text: #333;
      --muted: #666; --success: #28a745; --danger: #dc3545;
    }
    *{box-sizing:border-box;margin:0;padding:0;}
    body{font-family:sans-serif;background:var(--bg);
      display:flex;align-items:center;justify-content:center;
      height:100vh;}
    .card{background:var(--card);border-radius:8px;
      box-shadow:0 4px 12px rgba(0,0,0,0.1);
      max-width:420px;width:100%;overflow:hidden;}
    .hdr{background:linear-gradient(135deg,var(--brand),var(--brand-dark));
      padding:20px;text-align:center;color:#fff;}
    .hdr h1{font-size:1.5em;font-weight:400;}
    .body{padding:20px;}
    label{display:block;margin-top:12px;color:var(--muted);}
    input{margin-top:4px;width:100%;padding:8px;font-size:1em;
      border:1px solid #ccc;border-radius:4px;}
    .btns{display:flex;justify-content:space-between;margin-top:20px;}
    .btns button{flex:1;margin:0 5px;padding:12px;font-size:1em;
      border:none;border-radius:4px;color:#fff;cursor:pointer;}
    #update{background:var(--brand);} #reset{background:var(--danger);}
    #refresh{background:#007acc;} #scan{background:#28a745;}
    #progress{display:none;height:6px;width:100%;background:#eee;
      border-radius:3px;overflow:hidden;margin-top:16px;}
    #progress>div{height:100%;width:0;background:var(--success);
      transition:width 0.2s ease;}
    #status{margin-top:8px;text-align:center;font-size:0.9em;}
    #scanResults{margin-top:12px;font-size:0.9em;color:var(--text);}
  </style>
</head><body>
  <div class="card">
    <div class="hdr"><h1>TDS8 Network Config</h1></div>
    <div class="body">
      <form id="form">
        <label>IP Address<input type="text" id="ip"   value=")rawliteral" + ip + R"rawliteral("></label>
        <label>Gateway   <input type="text" id="gw"   value=")rawliteral" + gw + R"rawliteral("></label>
        <label>Subnet    <input type="text" id="nm"   value=")rawliteral" + nm + R"rawliteral("></label>
        <label>DNS 1     <input type="text" id="dns1" value=")rawliteral" + d1 + R"rawliteral("></label>
        <label>DNS 2     <input type="text" id="dns2" value=")rawliteral" + d2 + R"rawliteral("></label>

        <div class="btns">
          <button type="button" id="refresh">Refresh</button>
          <button type="submit" id="update">Update</button>
          <button type="button" id="reset">Reset</button>
          <button type="button" id="scan">Scan Networks</button>
        </div>

        <div id="progress"><div></div></div>
        <div id="status"></div>
        <pre id="scanResults"></pre>
      </form>
    </div>
  </div>

  <script>
    const f   = document.getElementById('form'),
          ip  = document.getElementById('ip'),
          gw  = document.getElementById('gw'),
          nm  = document.getElementById('nm'),
          dns1= document.getElementById('dns1'),
          dns2= document.getElementById('dns2'),
          upd = document.getElementById('update'),
          rst = document.getElementById('reset'),
          ref = document.getElementById('refresh'),
          scan= document.getElementById('scan'),
          prog= document.getElementById('progress'),
          bar = prog.querySelector('div'),
          st  = document.getElementById('status'),
          sr  = document.getElementById('scanResults');

    function busy(on){
      [upd,rst,ref,scan].forEach(b=>b.disabled=on);
      prog.style.display = on?'block':'none';
      if(on){ bar.style.width='0%'; st.textContent='Working…'; }
      else setTimeout(()=>st.textContent='',1500);
    }

    function populate(json){
      ip.value   = json.ip;
      gw.value   = json.gw;
      nm.value   = json.nm;
      dns1.value = json.dns1;
      dns2.value = json.dns2;
    }

    function go(path,data, isUpdate){
      busy(true);
      fetch(path,{
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body:JSON.stringify(data)
      })
      .then(r=>r.json())
      .then(json=>{
        populate(json);
        st.textContent = json.result;
        if(isUpdate && json.result==='ok'){
          setTimeout(()=>window.location.href='http://'+json.ip+'/',800);
        }
      })
      .finally(()=> busy(false));
    }

    f.addEventListener('submit', e=>{
      e.preventDefault();
      go('/config',{
        ip:ip.value,gw:gw.value,nm:nm.value,
        dns1:dns1.value,dns2:dns2.value
      }, true);
    });

    rst.addEventListener('click', ()=> go('/reset', {}, false));
    ref.addEventListener('click', ()=>{
      busy(true);
      fetch('/status')
        .then(r=>r.json())
        .then(json=>{ populate(json); st.textContent='status'; })
        .finally(()=> busy(false));
    });

    scan.addEventListener('click', ()=>{
      busy(true);
      fetch('/scan')
        .then(r=>r.json())
        .then(list=>{
          sr.textContent = list.map(o=>o.ssid+' ('+o.rssi+'dBm)').join('\n');
        })
        .finally(()=> busy(false));
    });
  </script>
</body></html>
)rawliteral";

  server.send(200, "text/html", html);
}


void handleConfigHTTP() {
  // … JSON parse / Wi-Fi reconnect / saveConfig() …

  // broadcast the new IP as ASCII on port 9000
  {
    IPAddress g = WiFi.gatewayIP();
    IPAddress bcast(g[0],g[1],g[2],255);
    String ipStr = WiFi.localIP().toString();
    Udp.beginPacket(bcast, ipBroadcastPort);
    Udp.print(ipStr);
    Udp.endPacket();
    Serial.printf("🔊 Broadcast IP %s → %s:%d\n",
                  ipStr.c_str(), bcast.toString().c_str(), ipBroadcastPort);
  }

  // reply with full JSON so the form repopulates correctly
  String reply = String("{\"result\":\"ok\",")
    + "\"ip\":\""   + currentIP.toString()   + "\","
    + "\"gw\":\""   + currentGW.toString()   + "\","
    + "\"nm\":\""   + currentNM.toString()   + "\","
    + "\"dns1\":\"" + currentDNS1.toString() + "\","
    + "\"dns2\":\"" + currentDNS2.toString() + "\"}"
    ;
  server.send(200, "application/json", reply);
}


void handleReset() {
  // 1) restore defaults in RAM
  currentIP   = defaultIP;
  currentGW   = defaultGateway;
  currentNM   = defaultSubnet;
  currentDNS1 = defaultDNS1;
  currentDNS2 = defaultDNS2;

  // 2) apply & reconnect
  WiFi.disconnect(true);
  WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  saveConfig();

  // 3) broadcast the default IP as ASCII
  {
    IPAddress g = WiFi.gatewayIP();
    IPAddress bcast(g[0],g[1],g[2],255);
    String ipStr = WiFi.localIP().toString();
    Udp.beginPacket(bcast, ipBroadcastPort);
    Udp.print(ipStr);
    Udp.endPacket();
    Serial.printf("🔊 Broadcast IP %s → %s:%d\n",
                  ipStr.c_str(), bcast.toString().c_str(), ipBroadcastPort);
  }

  // 4) send JSON so the page repopulates with the defaults
  String reply = String("{\"result\":\"reset\",")
    + "\"ip\":\""   + currentIP.toString()   + "\","
    + "\"gw\":\""   + currentGW.toString()   + "\","
    + "\"nm\":\""   + currentNM.toString()   + "\","
    + "\"dns1\":\"" + currentDNS1.toString() + "\","
    + "\"dns2\":\"" + currentDNS2.toString() + "\"}"
    ;
  server.send(200, "application/json", reply);
}




//────────────────────────────────────────────────────────
// NVS persistence
//────────────────────────────────────────────────────────
void loadConfig(){
  prefs.begin("net", false);
  String s;
  s = prefs.getString("ip",   defaultIP     .toString());   currentIP  .fromString(s);
  s = prefs.getString("gw",   defaultGateway.toString());   currentGW  .fromString(s);
  s = prefs.getString("nm",   defaultSubnet .toString());   currentNM  .fromString(s);
  s = prefs.getString("dns1", defaultDNS1   .toString());   currentDNS1.fromString(s);
  s = prefs.getString("dns2", defaultDNS2   .toString());   currentDNS2.fromString(s);
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
