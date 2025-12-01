#include <SPI.h>
#include "epd5in79b.h"

Epd epd;

void drawTestPattern() {
  // Use the display dimensions from the driver
  const uint16_t width = EPD_WIDTH;
  const uint16_t height = EPD_HEIGHT;
  const uint16_t bytesPerLine = (width + 7) / 8;
  const uint32_t bufferSize = (uint32_t)bytesPerLine * height;

  // Allocate buffers for black and red/yellow planes
  uint8_t *black = (uint8_t *)malloc(bufferSize);
  uint8_t *color = (uint8_t *)malloc(bufferSize);
  if (!black || !color) {
    Serial.println("Buffer alloc failed");
    return;
  }
  memset(black, 0xFF, bufferSize);
  memset(color, 0xFF, bufferSize);

  // Draw simple content: black border and diagonal, color text band
  auto setPixel = [&](uint16_t x, uint16_t y, bool blackPixel, bool colorPixel) {
    if (x >= width || y >= height) return;
    uint32_t idx = y * bytesPerLine + (x / 8);
    uint8_t mask = 0x80 >> (x % 8);
    if (blackPixel) {
      black[idx] &= ~mask;
    }
    if (colorPixel) {
      color[idx] &= ~mask;
    }
  };

  // Black border
  for (uint16_t x = 0; x < width; ++x) {
    setPixel(x, 0, true, false);
    setPixel(x, height - 1, true, false);
  }
  for (uint16_t y = 0; y < height; ++y) {
    setPixel(0, y, true, false);
    setPixel(width - 1, y, true, false);
  }

  // Black diagonal lines
  for (uint16_t x = 0; x < width && x < height; ++x) {
    setPixel(x, x, true, false);
    setPixel(width - 1 - x, x, true, false);
  }

  // Color band with text-like pattern (simple blocks)
  for (uint16_t y = height / 3; y < height / 3 + 40 && y < height; ++y) {
    for (uint16_t x = 10; x < width - 10; ++x) {
      if (((x / 10) % 2) == 0) {
        setPixel(x, y, false, true);
      }
    }
  }

  Serial.println("Sending frame...");
  epd.DisplayFrame(black, color);
  free(black);
  free(color);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("Waveshare 5.79in e-Paper (B) test on XIAO ESP32-C3");

  if (epd.Init() != 0) {
    Serial.println("EPD init failed");
    return;
  }

  Serial.println("Clearing display...");
  epd.Clear();
  delay(1000);

  Serial.println("Drawing test pattern...");
  drawTestPattern();

  Serial.println("Putting panel to sleep");
  epd.Sleep();
}

void loop() {
  // Intentionally left empty
}
