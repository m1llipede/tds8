#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <OSCMessage.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include "logo.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define TCA_ADDR 0x70

// State machine for startup sequence
enum AppState {
  STATE_STARTUP_SPLASH,
  STATE_INSTRUCTIONS,
  STATE_RUNNING
};
AppState currentState = STATE_STARTUP_SPLASH;
unsigned long stateTransitionTime = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);
Preferences prefs;
WiFiUDP Udp;

const char* FW_VERSION = "1.0.0";
const unsigned int oscPort = 8888;
const unsigned int ipBroadcastPort = 9000;
const uint8_t numScreens = 8;

String trackNames[numScreens];
int actualTrackNumbers[numScreens];
int activeTrack = -1;

// --- Function Declarations ---
void tcaSelect(uint8_t i);
void drawTrackName(uint8_t screen, const String& name);
void refreshAll();
void showStartupSplash();
void showQuickStartInstructions();
void handleSerialLine(const String& line);

void setup() {
  Serial.begin(115200);
  while (!Serial) {;}
  Serial.println("\n\nBooting TDS-8...");

  Wire.begin();
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.printf("Screen %d allocation failed\n", i);
    }
    display.clearDisplay();
    display.display();
  }

  // Show the initial TDS-8 logo splash screen
  showStartupSplash();
  stateTransitionTime = millis() + 3000; // Hold splash for 3 seconds
  currentState = STATE_STARTUP_SPLASH;

  // Load saved track names
  prefs.begin("tracks", true);
  for (int i = 0; i < numScreens; i++) {
    String key = "name" + String(i);
    trackNames[i] = prefs.getString(key.c_str(), "");
    actualTrackNumbers[i] = i;
  }
  prefs.end();

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  if (!wm.autoConnect("TDS-8-Setup")) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
  }

  Serial.print("WiFi Connected. IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("tds8")) {
    Serial.println("MDNS responder started");
    MDNS.addService("http", "tcp", 80);
  }

  Udp.begin(oscPort);
  Serial.printf("OSC listening on UDP %u\n", oscPort);
}

void loop() {
  // State machine for startup sequence
  switch (currentState) {
    case STATE_STARTUP_SPLASH:
      if (millis() >= stateTransitionTime) {
        Serial.println("⏱️ Splash screen complete. Showing instructions...");
        showQuickStartInstructions();
        stateTransitionTime = millis() + 5000; // Hold instructions for 5 seconds
        currentState = STATE_INSTRUCTIONS;
      }
      return; // Do nothing else during splash

    case STATE_INSTRUCTIONS:
      if (millis() >= stateTransitionTime) {
        Serial.println("✅ Instructions complete. Switching to track displays.");
        refreshAll(); // Show blank track names
        currentState = STATE_RUNNING;
      }
      return; // Do nothing else during instructions

    case STATE_RUNNING:
      // Normal operation starts here
      break; // Continue to the rest of the loop
  }

  // --- Main Loop Logic (only runs after startup sequence) ---
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    OSCMessage msg;
    while (packetSize--) {
      msg.fill(Udp.read());
    }
    if (!msg.hasError()) {
      msg.dispatch("/trackname", [](OSCMessage &msg) {
        if (msg.size() < 2) return;
        int idx = msg.getInt(0);
        char buf[64];
        msg.getString(1, buf, sizeof(buf));
        if (idx >= 0 && idx < numScreens) {
          trackNames[idx] = String(buf);
          drawTrackName(idx, trackNames[idx]);
        }
      });
    }
  }

  while (Serial.available() > 0) {
    static String serialBuffer = "";
    char c = (char)Serial.read();
    if (c == '\n') {
      handleSerialLine(serialBuffer);
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }
}

void handleSerialLine(const String& line) {
    if (line.startsWith("/reannounce")) {
        Serial.println("🔄 Received /reannounce, forwarding to M4L...");
        OSCMessage msgReannounce("/reannounce");
        IPAddress gw = WiFi.gatewayIP();
        IPAddress bcast(gw[0], gw[1], gw[2], 255);
        Udp.beginPacket(bcast, ipBroadcastPort);
        msgReannounce.send(Udp);
        Udp.endPacket();
    }
}

void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

void showStartupSplash() {
  const char* letters[] = {"T", "D", "S", "-", "8"};
  for (uint8_t i = 0; i < 5; i++) {
    tcaSelect(i + 1);
    display.clearDisplay();
    display.setTextSize(6);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(letters[i], 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.println(letters[i]);
    display.display();
  }
}

void showQuickStartInstructions() {
  const char* lines1[] = {"Launch", "Drag", "Browser:", "Hit"};
  const char* lines2[] = {"Ableton", "TDS8.amxd", "tds8.local", "Refresh"};
  for (uint8_t i = 0; i < 4; i++) {
    tcaSelect(i);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(5, 15);
    display.println(lines1[i]);
    display.setCursor(5, 35);
    display.println(lines2[i]);
    display.display();
  }
}

void refreshAll() {
  for (uint8_t i = 0; i < numScreens; i++) {
    drawTrackName(i, trackNames[i]);
  }
}

void drawTrackName(uint8_t screen, const String& name) {
  if (screen >= numScreens) return;
  tcaSelect(screen);
  display.clearDisplay();
  display.setTextWrap(true);
  display.setTextColor(SSD1306_WHITE);

  // Draw Track Name
  display.setTextSize(2);
  display.setCursor(2, 10);
  display.println(name);

  // Draw Track Number at bottom right
  display.setTextSize(1);
  String trackNumStr = String(actualTrackNumbers[screen] + 1);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(trackNumStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - w - 4, SCREEN_HEIGHT - h - 4);
  display.println(trackNumStr);

  display.display();
}