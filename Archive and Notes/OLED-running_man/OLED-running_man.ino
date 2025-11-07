#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define TCA_ADDRESS 0x70
#define NUM_SCREENS 8
#define BEZEL_WIDTH 40  // Simulated physical gap between OLEDs
#define I2C_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastUpdate = 0;
const int frameInterval = 0;  // ms between frames

// Ball state
int ballX = 64;
int ballY = 32;
int ballVX = 25;
int ballVY = 5;
const int ballRadius = 12;

void tcaSelect(uint8_t channel) {
  if (channel > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

void clearAllScreens() {
  for (uint8_t i = 0; i < NUM_SCREENS; i++) {
    tcaSelect(i);
    display.clearDisplay();
    display.display();
  }
}

void drawBouncingBall() {
  // Clear and update only the screen where the ball is
  for (uint8_t i = 0; i < NUM_SCREENS; i++) {
    int screenStartX = i * (SCREEN_WIDTH + BEZEL_WIDTH);
    int screenEndX = screenStartX + SCREEN_WIDTH;

    if (ballX + ballRadius >= screenStartX && ballX - ballRadius < screenEndX) {
      int localX = ballX - screenStartX;

      tcaSelect(i);
      display.clearDisplay();
      display.fillCircle(localX, ballY, ballRadius, SSD1306_WHITE);
      display.display();
    } else {
      tcaSelect(i);
      display.clearDisplay();
      display.display();
    }
  }
}

void updateBall() {
  int totalWidth = NUM_SCREENS * SCREEN_WIDTH + (NUM_SCREENS - 1) * BEZEL_WIDTH;

  // Move
  ballX += ballVX;
  ballY += ballVY;

  // Bounce off sides (virtual screen space)
  if (ballX - ballRadius < 0 || ballX + ballRadius > totalWidth) {
    ballVX *= -1;
  }

  // Bounce off top/bottom
  if (ballY - ballRadius < 0 || ballY + ballRadius > SCREEN_HEIGHT) {
    ballVY *= -1;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
    Serial.println("Display init failed");
    while (1);
  }

  clearAllScreens();
}

void loop() {
  unsigned long now = millis();
  if (now - lastUpdate >= frameInterval) {
    lastUpdate = now;

    updateBall();
    drawBouncingBall();
  }
}
