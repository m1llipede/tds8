#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define TCA_ADDRESS 0x70

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Select a specific channel on the TCA9548A
void tcaSelect(uint8_t channel) {
  if (channel > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

// Draw "Track X" on the given screen
void drawTrack(uint8_t screenNum) {
  tcaSelect(screenNum);
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Track %d", screenNum + 1);
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Loop through TCA channels 0–3
  for (uint8_t i = 0; i < 4; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.printf("SSD1306 failed on channel %d\n", i);
    } else {
      drawTrack(i);
      delay(300);  // Let it show before switching channels
    }
  }
}

void loop() {
  // Nothing in loop — we’re just testing display
}
