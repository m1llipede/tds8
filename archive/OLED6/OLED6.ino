#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>

// OLED and I2C Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_ADDR      0x3C
#define TCA_ADDRESS   0x70
const int numScreens = 8;

// WiFi Settings (default)
const char* ssid = "Cardinal";
const char* password = "11077381";

// Default static IP config (change as needed)
IPAddress defaultIP(192, 168, 68, 180);
IPAddress defaultGateway(192, 168, 68, 1);
IPAddress defaultSubnet(255, 255, 255, 0);

// OSC Settings
WiFiUDP Udp;
const unsigned int localPort = 8000;

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

  // Draw border
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
  msg.getString(1, buf, 64);
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
    for (int i = 0; i < numScreens; i++) {
      drawTrackName(i, trackNames[i]);
    }
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
  if (msg.size() >= 4) { msg.getString(3, dns1Buf, sizeof(dns1Buf)); dns1.fromString(dns1Buf); }
  if (msg.size() >= 5) { msg.getString(4, dns2Buf, sizeof(dns2Buf)); dns2.fromString(dns2Buf); }

  WiFi.disconnect(true);
  if (WiFi.config(ip, gw, nm, dns1, dns2)) {
    Serial.printf("📡 Static IP: %s, GW: %s, Mask: %s\n",
      ipBuf, gwBuf, nmBuf);
  } else {
    Serial.println("❌ Failed to set static IP");
  }
  Serial.print("Reconnecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("✅");
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  // Static IP by default
  if (WiFi.config(defaultIP, defaultGateway, defaultSubnet)) {
    Serial.printf("📡 Using static IP: %s\n",
      defaultIP.toString().c_str());
  } else {
    Serial.println("⚠️ Static IP config failed");
  }
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());

  Udp.begin(localPort);
  Serial.printf("🛰️ OSC port %d\n", localPort);

  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR))
      Serial.printf("❌ OLED init failed %d\n", i);
    else
      drawTrackName(i, trackNames[i]);
  }
}

void loop() {
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
