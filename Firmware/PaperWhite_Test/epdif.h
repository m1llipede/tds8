#ifndef EPDIF_H
#define EPDIF_H

#include <Arduino.h>

#define RST_PIN   8   // D8
#define DC_PIN    9   // D9
#define CS_PIN   10   // D10
#define BUSY_PIN  7   // D7
#define PWR_PIN   4   // D4, or comment out if PWR is hard-wired high

class EpdIf {
public:
    static int  IfInit(void);
    static void DigitalWrite(int pin, int value);
    static int  DigitalRead(int pin);
    static void DelayMs(unsigned int delaytime);
    static void SpiTransfer(unsigned char data);
};

#endif
