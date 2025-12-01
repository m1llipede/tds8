#ifndef EPD5IN79B_H
#define EPD5IN79B_H

#include <Arduino.h>

#define EPD_WIDTH  792
#define EPD_HEIGHT 272

class Epd {
public:
    Epd(void);
    int  Init(void);
    void Clear(void);
    void Sleep(void);

    void DisplayPartBaseImage(const unsigned char* image);
    void Displaypart(const unsigned char* image, unsigned char block);
    void DisplayFrame(const uint8_t* black, const uint8_t* color);

    // low-level helpers used internally
    void Reset(void);
    void SendCommand(unsigned char command);
    void SendData(unsigned char data);
    void WaitUntilIdle(void);
    void SetWindows(int x_start, int y_start, int x_end, int y_end);
    void SetCursor(int x, int y);

    // public low-level data write (used by your file-stream function)
    inline void EPD_W21_WriteDATA(uint8_t data) {
        SendData(data);
    }

    inline void EPD_WhiteScreen_White(void) {
        SendCommand(0x24);
    }

public:
    unsigned int width;
    unsigned int height;

private:
    int reset_pin;
    int dc_pin;
    int cs_pin;
    int busy_pin;
    int pwr_pin;
};

#endif
