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

// Static IP config
IPAddress local_IP(192, 168, 68, 200);
IPAddress gateway(192, 168, 68, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

WiFiUDP Udp;
const unsigned int localPort = 8000;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int numScreens = 8;
String trackNames[numScreens];

void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);
  Wire.endTransmission();
}

void drawTrackName(uint8_t screen, const String& name) {
  tcaSelect(screen);
  display.clearDisplay();

  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);

  if (name.length() > 0) {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(name, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (SCREEN_WIDTH - w) / 2;
    int16_t y = (SCREEN_HEIGHT - h) / 2;

    display.setCursor(x, y);
    display.println(name);
  }

  display.display();
}

void handleTrackName(OSCMessage &msg) {
  if (msg.size() < 2) return;
  int index = msg.getInt(0);
  char buf[64];
  msg.getString(1, buf, 64);
  String newName = String(buf);

  if (index >= 0 && index < numScreens) {
    // Only update if name is not empty or whitespace
    newName.trim();
    if (newName.length() > 0) {
      trackNames[index] = newName;
      Serial.printf("Updated Track %d: %s\n", index + 1, buf);
    } else {
      Serial.printf("Empty name received for Track %d — ignored\n", index + 1);
    }

    drawTrackName(index, trackNames[index]);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Static IP
  if (!WiFi.config(local_IP, gateway, subnet, dns)) {
    Serial.println("Failed to configure static IP");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("ESP IP Address: ");
  Serial.println(WiFi.localIP());

  Udp.begin(localPort);
  Serial.printf("Listening for OSC on port %d\n", localPort);

  // Initialize track names with defaults
  for (int i = 0; i < numScreens; i++) {
    trackNames[i] = "Track " + String(i + 1);
  }

  // Draw initial names
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.printf("SSD1306 failed on channel %d\n", i);
    } else {
      drawTrackName(i, trackNames[i]);
    }
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
