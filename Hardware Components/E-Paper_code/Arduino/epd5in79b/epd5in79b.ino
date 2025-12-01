/**
 *  @filename   :   epd5in79b-demo.ino
 *  @brief      :   5.79inch e-paper (B) display demo
 *  @author     :   MyMX from Waveshare
 *
 *  Copyright (C) Waveshare     2024/03/06
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER bIN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <SPI.h>
#include "epd5in79b.h"
#include "imagedata.h"
#include <stdlib.h>
#include <avr/pgmspace.h>

Epd epd;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("Init EPD...");
    if (epd.Init() != 0) {
        Serial.println("EPD init failed");
        return;
    }

    Serial.println("Clearing...");
    unsigned long t0 = millis();
    epd.Clear();
    unsigned long t1 = millis();
    Serial.print("Clear time (ms): ");
    Serial.println(t1 - t0);

    Serial.println("Display IMAGE_DATA...");
    unsigned long t2 = millis();
    epd.Displaypart(IMAGE_DATA, 0);
    epd.Displaypart(IMAGE_DATA, 1);
    unsigned long t3 = millis();
    Serial.print("Displaypart time (ms): ");
    Serial.println(t3 - t2);

    Serial.println("Done, going to sleep");
    epd.Sleep();
}

void loop() {
}

