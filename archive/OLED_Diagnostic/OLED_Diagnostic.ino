#include <Wire.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define TCA_ADDRESS 0x70

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  for (uint8_t ch = 0; ch < 8; ch++) {
    Serial.printf("Testing TCA channel %d...\n", ch);
    tcaSelect(ch);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.printf("❌ OLED not found on channel %d\n", ch);
    } else {
      Serial.printf("✅ OLED FOUND on channel %d\n", ch);
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 20);
      display.printf("CH %d", ch);
      display.display();
      delay(1000);
      display.clearDisplay();
    }
  }
}

void loop() {}
