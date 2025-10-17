#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define TCA_ADDRESS 0x70

const char* ssid = "Cardinal";
const char* password = "11077381";

WiFiUDP Udp;
const unsigned int localPort = 8000;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int numScreens = 8;
String trackNames[numScreens] = {
  "Track 1", "Track 2", "Track 3", "Track 4",
  "Track 5", "Track 6", "Track 7", "Track 8"
};

void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);
  Wire.endTransmission();
}

void drawTrackName(uint8_t screen, const String& name) {
  tcaSelect(screen);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(name);
  display.display();
}

void handleTrackName(OSCMessage &msg) {
  if (msg.size() < 2) return;
  int index = msg.getInt(0);
  char buf[64];
  msg.getString(1, buf, 64);
  if (index >= 0 && index < numScreens) {
    trackNames[index] = String(buf);
    drawTrackName(index, trackNames[index]);
    Serial.printf("Updated Track %d: %s\n", index + 1, buf);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Udp.begin(localPort);
  Serial.printf("Listening for OSC on port %d\n", localPort);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 initialization failed!");
    while (true);
  }

  for (uint8_t i = 0; i < numScreens; i++) {
    drawTrackName(i, trackNames[i]);
    delay(200);  // Give each screen a moment to update (optional)
  }
}

void loop() {
  OSCMessage msg;
  int packetSize = Udp.parsePacket();
  if (packetSize > 0) {
    while (packetSize--) {
      msg.fill(Udp.read());
    }
    if (!msg.hasError()) {
      msg.dispatch("/trackname", handleTrackName);
    } else {
      Serial.println("OSC message error.");
    }
  }
}
