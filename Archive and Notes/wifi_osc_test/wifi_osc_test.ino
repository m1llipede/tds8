#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>

// Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define I2C_ADDR 0x3C
#define TCA_ADDRESS 0x70
const int numScreens = 8;

// WiFi Settings
const char* ssid = "Cardinal";
const char* password = "11077381";

// OSC Settings
WiFiUDP Udp;
const unsigned int localPort = 8000;

// Display Object (reused across screens)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Track Names
String trackNames[numScreens] = {
  "Track 1", "Track 2", "Track 3", "Track 4",
  "Track 5", "Track 6", "Track 7", "Track 8"
};

// Select TCA9548A channel
void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);
  Wire.endTransmission();
}

// Draw track name or empty outline
void drawTrackName(uint8_t screen, const String& name) {
  tcaSelect(screen);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  String nameCopy = name;
  nameCopy.trim();

  if (nameCopy.length() == 0) {
    // Draw border only
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  } else {
    // Draw centered text
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(nameCopy, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;
    display.setCursor(x, y);
    display.println(nameCopy);
  }

  display.display();
}


// Handle OSC /trackname [index] [name]
void handleTrackName(OSCMessage &msg) {
  if (msg.size() < 2) return;

  int index = msg.getInt(0);
  char buf[64];
  msg.getString(1, buf, 64);
  String name = String(buf);

  if (index >= 0 && index < numScreens) {
    trackNames[index] = name;
    drawTrackName(index, trackNames[index]);
    Serial.printf("✅ Updated Track %d: %s\n", index + 1, buf);
  } else {
    Serial.printf("❌ Invalid track index: %d\n", index);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);  // Speed up I²C bus

  // WiFi Connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Start OSC UDP
  Udp.begin(localPort);
  Serial.printf("Listening for OSC on port %d\n", localPort);

  // Init Displays
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
      Serial.printf("❌ Display init failed on channel %d\n", i);
    } else {
      // Optionally clear some names for outline testing
      // trackNames[i] = "";  // Uncomment to simulate empty screen
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
