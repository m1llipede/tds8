#include <stdlib.h>
#include <SPI.h>
#include <pgmspace.h>
#include "epd5in79b.h"
#include "epdif.h"

Epd::Epd(void)
{
    reset_pin = RST_PIN;
    dc_pin    = DC_PIN;
    cs_pin    = CS_PIN;
    busy_pin  = BUSY_PIN;
    pwr_pin   = PWR_PIN;
    width  = EPD_WIDTH;
    height = EPD_HEIGHT;
}

int Epd::Init(void)
{
    Serial.println(F("[EPD] IfInit"));
    if (EpdIf::IfInit() != 0) {
        Serial.println(F("[EPD] IfInit failed"));
        return -1;
    }

    Serial.println(F("[EPD] Power enable"));
    EpdIf::DigitalWrite(pwr_pin, 1);
    EpdIf::DelayMs(100);

    Serial.println(F("[EPD] Reset"));
    Reset();

    Serial.println(F("[EPD] SWRESET"));
    SendCommand(0x12);   // SWRESET
    WaitUntilIdle();

    Serial.println(F("[EPD] Data entry mode"));
    SendCommand(0x11);   // data entry mode
    SendData(0x03);

    Serial.println(F("[EPD] Set RAM windows"));
    SendCommand(0x44);   // set RAM X start/end
    SendData(0x00);
    SendData((width - 1) / 8);

    SendCommand(0x45);   // set RAM Y start/end
    SendData(0x00);
    SendData(0x00);
    SendData((height - 1) & 0xFF);
    SendData(((height - 1) >> 8) & 0xFF);

    SendCommand(0x4E);   // RAM X pointer
    SendData(0x00);

    SendCommand(0x4F);   // RAM Y pointer
    SendData(0x00);
    SendData(0x00);

    Serial.println(F("[EPD] Init done"));
    WaitUntilIdle();
    return 0;
}

void Epd::Reset(void)
{
    EpdIf::DigitalWrite(reset_pin, 0);
    EpdIf::DelayMs(100);
    EpdIf::DigitalWrite(reset_pin, 1);
    EpdIf::DelayMs(100);
}

void Epd::SendCommand(unsigned char command)
{
    EpdIf::DigitalWrite(dc_pin, 0);
    EpdIf::DigitalWrite(cs_pin, 0);
    EpdIf::SpiTransfer(command);
    EpdIf::DigitalWrite(cs_pin, 1);
}

void Epd::SendData(unsigned char data)
{
    EpdIf::DigitalWrite(dc_pin, 1);
    EpdIf::DigitalWrite(cs_pin, 0);
    EpdIf::SpiTransfer(data);
    EpdIf::DigitalWrite(cs_pin, 1);
}

void Epd::WaitUntilIdle(void)
{
    unsigned long start = millis();
    while (EpdIf::DigitalRead(busy_pin) == 1) {
        EpdIf::DelayMs(100);
        if ((millis() - start) % 1000 == 0) {
            Serial.println(F("[EPD] Busy..."));
        }
    }
}

void Epd::SetWindows(int x_start, int y_start, int x_end, int y_end)
{
    SendCommand(0x44);
    SendData((x_start / 8) & 0xFF);
    SendData((x_end   / 8) & 0xFF);

    SendCommand(0x45);
    SendData(y_start & 0xFF);
    SendData((y_start >> 8) & 0xFF);
    SendData(y_end   & 0xFF);
    SendData((y_end   >> 8) & 0xFF);
}

void Epd::SetCursor(int x, int y)
{
    SendCommand(0x4E);
    SendData((x / 8) & 0xFF);

    SendCommand(0x4F);
    SendData(y & 0xFF);
    SendData((y >> 8) & 0xFF);

    WaitUntilIdle();
}

void Epd::Clear(void)
{
    int w = width / 8;
    int h = height;

    Serial.println(F("[EPD] Clear start"));
    SendCommand(0x24);
    for (int i = 0; i < w * h; i++)
        SendData(0xFF);

    SendCommand(0x22);
    SendData(0xF7);
    SendCommand(0x20);
    WaitUntilIdle();
    Serial.println(F("[EPD] Clear done"));
}

void Epd::DisplayFrame(const uint8_t* black, const uint8_t* color)
{
    int bytesPerLine = width / 8;
    int totalBytes = bytesPerLine * height;

    SendCommand(0x24);
    for (int i = 0; i < totalBytes; ++i) {
        SendData(black ? black[i] : 0xFF);
    }

    SendCommand(0x26);
    for (int i = 0; i < totalBytes; ++i) {
        SendData(color ? color[i] : 0xFF);
    }

    SendCommand(0x22);
    SendData(0xF7);
    SendCommand(0x20);
    WaitUntilIdle();
    Serial.println(F("[EPD] Frame displayed"));
}

void Epd::DisplayPartBaseImage(const unsigned char* image)
{
    int w = width / 8;
    int h = height;

    SendCommand(0x24);
    for (int i = 0; i < w * h; i++)
        SendData(pgm_read_byte(&image[i]));

    SendCommand(0x26);
    for (int i = 0; i < w * h; i++)
        SendData(pgm_read_byte(&image[i]));

    SendCommand(0x22);
    SendData(0xF7);
    SendCommand(0x20);
    WaitUntilIdle();
}

void Epd::Displaypart(const unsigned char* image, unsigned char block)
{
    int w = width / 8;
    int h = height;

    SendCommand(0x24);
    for (int i = 0; i < w * h; i++)
        SendData(pgm_read_byte(&image[i]));

    SendCommand(0x22);
    SendData(0xF7);
    SendCommand(0x20);
    WaitUntilIdle();
}

void Epd::Sleep(void)
{
    Serial.println(F("[EPD] Sleep"));
    SendCommand(0x10);
    SendData(0x01);
    EpdIf::DelayMs(100);
}
