#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>        // for DynamicJsonDocument

// OLED and I2C Settings
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET     -1
#define I2C_ADDR        0x3C
#define TCA_ADDRESS     0x70

Preferences prefs;
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
int prevActive = -1;  // for optimized redraw

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

  // apply saved (or default) config
  if (!WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2)) {
    Serial.println("⚠️ WiFi.config failed");
  }
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected! IP: " + WiFi.localIP().toString());

  // mDNS responder
  if (MDNS.begin(mdnsName)) {
    Serial.println("mDNS responder started: " + String(mdnsName) + ".local");
  }

  // HTTP routes
  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/config", HTTP_POST, handleConfigHTTP);
  server.on("/reset",  HTTP_POST, handleReset);
  server.begin();
  Serial.println("🌐 HTTP server started");

  // OSC listener
  Udp.begin(localPort);
  Serial.printf("🛰️ OSC port %d\n", localPort);

  // Init OLED screens
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
// I2C OLED & TCA9548A helper functions
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
  display.drawRect(0, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1,
                   inverted ? SSD1306_BLACK : SSD1306_WHITE);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds(name,0,0,&x1,&y1,&w,&h);
  display.setCursor((SCREEN_WIDTH-w)/2, (SCREEN_HEIGHT-h)/2);
  display.println(name);
  display.display();
}


//────────────────────────────────────────────────────────
// OSC handlers
//────────────────────────────────────────────────────────
void handleTrackName(OSCMessage &msg) {
  if (msg.size() < 2) return;
  int index = msg.getInt(0);
  char buf[64];
  msg.getString(1, buf, sizeof(buf));
  if (index>=0 && index<numScreens) {
    trackNames[index] = String(buf);
    drawTrackName(index, trackNames[index]);
    Serial.printf("✅ Track %d → %s\n", index, buf);
  }
}

void handleActiveTrack(OSCMessage &msg) {
  if (msg.size() < 1) return;
  int newIndex = msg.getInt(0);
  if (newIndex<0 || newIndex>=numScreens) return;
  if (newIndex == activeTrack) return;  // no change
  Serial.printf("🎯 Active Track: %d\n", newIndex);
  int oldIndex = activeTrack;
  activeTrack = newIndex;
  if (oldIndex>=0 && oldIndex<numScreens)
    drawTrackName(oldIndex, trackNames[oldIndex]);
  drawTrackName(activeTrack, trackNames[activeTrack]);
}

void handleIPConfig(OSCMessage &msg) {
  // your existing OSC /ipconfig handler...
}


//────────────────────────────────────────────────────────
// HTTP handlers
//────────────────────────────────────────────────────────
void handleRoot(){
  // your existing handleRoot() HTML…
}

void handleConfigHTTP() {
  // … your JSON parse / Wi-Fi reconnect / saveConfig() …

  // — RAW UDP broadcast on port 9000 —
  {
    IPAddress gw = WiFi.gatewayIP();
    IPAddress bcast(gw[0], gw[1], gw[2], 255);
    String ipStr = WiFi.localIP().toString();
    Udp.beginPacket(bcast, ipBroadcastPort);
    Udp.print(ipStr);
    Udp.endPacket();
    Serial.printf("🔊 Raw UDP broadcast %s → %s:%d\n",
                  ipStr.c_str(), bcast.toString().c_str(), ipBroadcastPort);
  }

  // — OSC broadcast /ipupdate on port 9000 —
  {
    IPAddress gw = WiFi.gatewayIP();
    IPAddress bcast(gw[0], gw[1], gw[2], 255);
    String ipStr = WiFi.localIP().toString();
    OSCMessage m("/ipupdate");
    m.add(ipStr.c_str());
    Udp.beginPacket(bcast, ipBroadcastPort);
    m.send(Udp);
    Udp.endPacket();
    Serial.printf("🎺 OSC /ipupdate %s → %s:%d\n",
                  ipStr.c_str(), bcast.toString().c_str(), ipBroadcastPort);
  }

  // build a JSON reply that includes every field
  String reply = String("{\"result\":\"ok\","
    "\"ip\":\"")   + currentIP.toString()   + "\","
    "\"gw\":\"")  + currentGW.toString()   + "\","
    "\"nm\":\"")  + currentNM.toString()   + "\","
    "\"dns1\":\"")+ currentDNS1.toString() + "\","
    "\"dns2\":\"")+ currentDNS2.toString() + "\"}";
  server.send(200, "application/json", reply);
  }

void handleReset() {
  // — RAW UDP broadcast on port 9000 —
  {
    IPAddress gw = WiFi.gatewayIP();
    IPAddress bcast(gw[0], gw[1], gw[2], 255);
    String ipStr = WiFi.localIP().toString();
    Udp.beginPacket(bcast, ipBroadcastPort);
    Udp.print(ipStr);
    Udp.endPacket();
    Serial.printf("🔊 Raw UDP broadcast %s → %s:%d\n",
                  ipStr.c_str(), bcast.toString().c_str(), ipBroadcastPort);
  }

  // — OSC broadcast /ipupdate on port 9000 —
  {
    IPAddress gw = WiFi.gatewayIP();
    IPAddress bcast(gw[0], gw[1], gw[2], 255);
    String ipStr = WiFi.localIP().toString();
    OSCMessage m("/ipupdate");
    m.add(ipStr.c_str());
    Udp.beginPacket(bcast, ipBroadcastPort);
    m.send(Udp);
    Udp.endPacket();
    Serial.printf("🎺 OSC /ipupdate %s → %s:%d\n",
                  ipStr.c_str(), bcast.toString().c_str(), ipBroadcastPort);
  }

  String reply = String("{\"result\":\"reset\","
    "\"ip\":\"")   + currentIP.toString()   + "\","
    "\"gw\":\"")  + currentGW.toString()   + "\","
    "\"nm\":\"")  + currentNM.toString()   + "\","
    "\"dns1\":\"")+ currentDNS1.toString() + "\","
    "\"dns2\":\"")+ currentDNS2.toString() + "\"}";
  server.send(200, "application/json", reply);}


//────────────────────────────────────────────────────────
// NVS persistence
//────────────────────────────────────────────────────────
void loadConfig(){
  prefs.begin("net", false);
  String s;
  s = prefs.getString("ip",   defaultIP     .toString()); currentIP  .fromString(s);
  s = prefs.getString("gw",   defaultGateway.toString()); currentGW  .fromString(s);
  s = prefs.getString("nm",   defaultSubnet .toString()); currentNM  .fromString(s);
  s = prefs.getString("dns1", defaultDNS1   .toString()); currentDNS1.fromString(s);
  s = prefs.getString("dns2", defaultDNS2   .toString()); currentDNS2.fromString(s);
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
