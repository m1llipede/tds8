#include "epdif.h"
#include <SPI.h>

int EpdIf::IfInit(void)
{
    pinMode(CS_PIN, OUTPUT);
    pinMode(RST_PIN, OUTPUT);
    pinMode(DC_PIN, OUTPUT);
    pinMode(PWR_PIN, OUTPUT);
    pinMode(BUSY_PIN, INPUT);

    SPI.begin(8, -1, 10, CS_PIN);
    return 0;
}

void EpdIf::DigitalWrite(int pin, int value)
{
    digitalWrite(pin, value);
}

int EpdIf::DigitalRead(int pin)
{
    return digitalRead(pin);
}

void EpdIf::DelayMs(unsigned int delaytime)
{
    delay(delaytime);
}

void EpdIf::SpiTransfer(unsigned char data)
{
    SPI.transfer(data);
}
