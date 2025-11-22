#include <Wire.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define TCA_ADDRESS 0x70

#define DIAG_VERSION "diagnostic_v1.0"

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

  Serial.println();
  Serial.println("=== TDS-8 OLED Diagnostic ===");
  Serial.print("VERSION: "); Serial.println(DIAG_VERSION);

  for (uint8_t ch = 0; ch < 8; ch++) {
    Serial.printf("Testing TCA channel %d...\n", ch);
    tcaSelect(ch);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.printf("❌ OLED not found on channel %d\n", ch);
    } else {
      Serial.printf("✅ OLED FOUND on channel %d\n", ch);
      display.clearDisplay();
      display.display();
    }
  }
}

// Simple fast animations per OLED to exercise pixels/geometry
static uint32_t frame = 0;

static void animRects(uint8_t ch, uint32_t f) {
  int w = 16 + ((f / 3 + ch * 7) % 96);
  int h = 8 + ((f / 2 + ch * 5) % 48);
  int x = (f + ch * 9) % (SCREEN_WIDTH - w);
  int y = ((f / 4) + ch * 11) % (SCREEN_HEIGHT - h);
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
}

static void animLines(uint8_t ch, uint32_t f) {
  int y1 = (f + ch * 3) % SCREEN_HEIGHT;
  int y2 = (SCREEN_HEIGHT - 1) - ((f / 2 + ch * 7) % SCREEN_HEIGHT);
  display.drawLine(0, y1, SCREEN_WIDTH - 1, y2, SSD1306_WHITE);
  int x1 = (f / 2 + ch * 13) % SCREEN_WIDTH;
  int x2 = (SCREEN_WIDTH - 1) - ((f / 3 + ch * 5) % SCREEN_WIDTH);
  display.drawLine(x1, 0, x2, SCREEN_HEIGHT - 1, SSD1306_WHITE);
}

static void animCircles(uint8_t ch, uint32_t f) {
  int r = 4 + ((f / 3 + ch * 2) % 28);
  display.drawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, r, SSD1306_WHITE);
  display.drawCircle((f / 2 + ch * 9) % SCREEN_WIDTH, (f / 3 + ch * 5) % SCREEN_HEIGHT, 6, SSD1306_WHITE);
}

static void animChecker(uint8_t ch, uint32_t f) {
  int s = 8; // cell size
  int ox = (f / 3 + ch * 2) % s;
  int oy = (f / 5 + ch * 3) % s;
  for (int y = 0; y < SCREEN_HEIGHT; y += s) {
    for (int x = 0; x < SCREEN_WIDTH; x += s) {
      bool on = ((x / s) ^ (y / s) ^ (f / 8)) & 1;
      if (on) display.fillRect(x - ox, y - oy, s, s, SSD1306_WHITE);
    }
  }
}

void loop() {
  frame++;
  for (uint8_t ch = 0; ch < 8; ch++) {
    tcaSelect(ch);
    display.clearDisplay();
    // Different pattern per channel, all animated over time
    switch (ch % 4) {
      case 0: animRects(ch, frame); break;
      case 1: animLines(ch, frame); break;
      case 2: animCircles(ch, frame); break;
      case 3: animChecker(ch, frame); break;
    }
    display.display();
  }
  // Keep animations fast; avoid long delays
  delay(0);
}
