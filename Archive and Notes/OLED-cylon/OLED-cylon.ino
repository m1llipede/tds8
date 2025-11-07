#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define TCA_ADDRESS 0x70
#define NUM_SCREENS 8
#define I2C_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

uint8_t brightness[NUM_SCREENS] = {0};

int direction = 1;           // 1 = forward, -1 = backward
int currentIndex = 0;        // position of the "eye"
unsigned long lastUpdate = 0;

int speedInterval = 200;      // ⏱ Speed: ms between frame steps
uint8_t fadeAmount = 50;     // 🔧 How much brightness fades per frame (0–255)

void tcaSelect(uint8_t channel) {
  if (channel > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

void drawFrame() {
  for (uint8_t i = 0; i < NUM_SCREENS; i++) {
    tcaSelect(i);
    display.clearDisplay();

    uint8_t value = brightness[i];

    if (value > 0) {
      display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
      display.dim(value < 128);  // Dim for trail, full brightness for main
    }

    display.display();
  }
}

void fadeTrails() {
  for (uint8_t i = 0; i < NUM_SCREENS; i++) {
    if (brightness[i] >= fadeAmount) {
      brightness[i] -= fadeAmount;
    } else {
      brightness[i] = 0;
    }
  }
}

void updateCylon() {
  // Set current LED to full brightness
  brightness[currentIndex] = 255;

  // Update direction if at end
  if (currentIndex == NUM_SCREENS - 1) direction = -1;
  if (currentIndex == 0) direction = 1;

  currentIndex += direction;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
    Serial.println("Display init failed");
    while (1);
  }

  // Clear all displays
  for (uint8_t i = 0; i < NUM_SCREENS; i++) {
    tcaSelect(i);
    display.clearDisplay();
    display.display();
  }
}

void loop() {
  unsigned long now = millis();

  // Only update when enough time has passed
  if (now - lastUpdate >= speedInterval) {
    lastUpdate = now;
    updateCylon();    // move active light
    fadeTrails();     // fade everything else
    drawFrame();      // draw all screens
  }
}
