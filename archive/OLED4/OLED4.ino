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

// WiFi Settings
const char* ssid = "Cardinal";
const char* password = "11077381";

// OSC Settings
WiFiUDP Udp;
const unsigned int localPort = 8000;

// Shared Display Object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Track Info
String trackNames[numScreens] = { "Track 1", "Track 2", "Track 3", "Track 4", "Track 5", "Track 6", "Track 7", "Track 8" };
int activeTrack = -1;

// Select OLED on TCA9548A
void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);
  Wire.endTransmission();
}

// Draw track name centered, with optional inversion
void drawTrackName(uint8_t screen, const String& name) {
  tcaSelect(screen);
  display.clearDisplay();
  display.invertDisplay(screen == activeTrack);

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

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);  // Fast I2C

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  Serial.println(WiFi.localIP());

  Udp.begin(localPort);
  Serial.printf("🛰️ Listening for OSC on port %d\n", localPort);

  // Init displays
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
      Serial.printf("❌ OLED init failed on screen %d\n", i);
    } else {
      drawTrackName(i, trackNames[i]);
    }
  }
}

void loop() {
  OSCMessage msg;
  int packetSize = Udp.parsePacket();
  if (packetSize > 0) {
    while (packetSize--) msg.fill(Udp.read());
    if (!msg.hasError()) {
      msg.dispatch("/trackname", handleTrackName);
      msg.dispatch("/activetrack", handleActiveTrack);
    }
  }
}
