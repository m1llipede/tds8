# Adding Graphics/Logos to TDS-8 OLED Displays

## 🎨 Complete Guide to Adding Your PlayOptix Logo

---

## Step 1: Prepare Your Logo Image

### **Requirements:**
- **Format:** PNG, BMP, or JPG
- **Colors:** Black and white (or will be converted)
- **Recommended size:** 64x32 pixels (fits nicely on 128x64 display)
- **Max size:** 128x64 pixels (full display)

### **Tips:**
- Simple, bold designs work best on small OLEDs
- High contrast is important
- Avoid fine details (they won't show well)
- White areas = lit pixels, Black areas = off pixels

---

## Step 2: Convert Logo to Bitmap Array

### **Use image2cpp (Recommended):**

1. **Go to:** https://javl.github.io/image2cpp/

2. **Upload your logo:**
   - Click "Choose File"
   - Select your PlayOptix logo

3. **Configure settings:**
   - **Canvas size:** 64 x 32 (or your desired size)
   - **Background color:** Black
   - **Invert image colors:** Check if your logo looks inverted
   - **Brightness/Threshold:** Adjust slider until preview looks good
   - **Scaling:** Original or Scale to fit
   - **Center:** Horizontally and vertically (if desired)

4. **Code output settings:**
   - **Code output format:** Arduino code
   - **Draw mode:** Horizontal - 1 bit per pixel
   - **Identifier:** `playoptix_logo`

5. **Generate code:**
   - Click "Generate code"
   - Copy the entire array

---

## Step 3: Add Bitmap to Arduino Code

### **What You'll Get:**

The tool generates something like this:

```cpp
// 'playoptix_logo', 64x32px
const unsigned char playoptix_logo [] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  // ... many more lines ...
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
```

### **Where to Add It:**

In `OLED32.ino`, find the bitmap section (around line 60):

```cpp
// =======================  Graphics / Bitmaps  ==============
// PlayOptix logo - 64x32px (replace with your actual logo from image2cpp)
// To create: https://javl.github.io/image2cpp/
const unsigned char playoptix_logo [] PROGMEM = {
  // PASTE YOUR BITMAP ARRAY HERE
  0x00, 0x00, 0x00, 0x00, // ... etc
};

#define LOGO_WIDTH  64   // Change to your logo width
#define LOGO_HEIGHT 32   // Change to your logo height
```

**Replace:**
1. The placeholder bytes with your generated array
2. `LOGO_WIDTH` with your logo width
3. `LOGO_HEIGHT` with your logo height

---

## Step 4: The Code is Already Set Up!

The splash screen function already uses the logo:

```cpp
void showStartupSplash() {
  // ... TDS-8 letters on displays 2-6 ...
  
  // Display 7: PlayOptix logo
  tcaSelect(7);
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
  
  // Draw logo centered
  int logoX = (SCREEN_WIDTH - LOGO_WIDTH) / 2;
  int logoY = (SCREEN_HEIGHT - LOGO_HEIGHT) / 2;
  display.drawBitmap(logoX, logoY, playoptix_logo, LOGO_WIDTH, LOGO_HEIGHT, SSD1306_WHITE);
  
  display.display();
  delay(5000);
}
```

---

## Step 5: Upload and Test

1. **Replace the bitmap array** with your generated code
2. **Update LOGO_WIDTH and LOGO_HEIGHT** if different
3. **Upload to XIAO**
4. **Power on** and watch your logo appear!

---

## 🎨 Example: Different Logo Sizes

### **Small Logo (32x16):**
```cpp
#define LOGO_WIDTH  32
#define LOGO_HEIGHT 16
```
- Good for: Simple icons, small branding
- Leaves room for text

### **Medium Logo (64x32):**
```cpp
#define LOGO_WIDTH  64
#define LOGO_HEIGHT 32
```
- Good for: Balanced logo + text
- Current setup

### **Large Logo (96x48):**
```cpp
#define LOGO_WIDTH  96
#define LOGO_HEIGHT 48
```
- Good for: Bold branding
- Takes most of the display

### **Full Display (128x64):**
```cpp
#define LOGO_WIDTH  128
#define LOGO_HEIGHT 64
```
- Good for: Maximum impact
- No room for border

---

## 🔧 Advanced: Multiple Graphics

You can add multiple bitmaps:

```cpp
// PlayOptix logo
const unsigned char playoptix_logo [] PROGMEM = { /* ... */ };
#define LOGO_WIDTH  64
#define LOGO_HEIGHT 32

// Small icon
const unsigned char icon_small [] PROGMEM = { /* ... */ };
#define ICON_WIDTH  16
#define ICON_HEIGHT 16

// Use them:
display.drawBitmap(x, y, playoptix_logo, LOGO_WIDTH, LOGO_HEIGHT, SSD1306_WHITE);
display.drawBitmap(x, y, icon_small, ICON_WIDTH, ICON_HEIGHT, SSD1306_WHITE);
```

---

## 🐛 Troubleshooting

### **Logo doesn't appear:**
- Check that bitmap array is complete (no missing bytes)
- Verify LOGO_WIDTH and LOGO_HEIGHT match your image
- Make sure `PROGMEM` is included in declaration

### **Logo is inverted (black/white swapped):**
- In image2cpp, toggle "Invert image colors"
- Or change `SSD1306_WHITE` to `SSD1306_BLACK` in drawBitmap()

### **Logo is cut off:**
- Check your logo size doesn't exceed 128x64
- Verify centering calculations are correct

### **Logo looks distorted:**
- Make sure your source image has correct aspect ratio
- Try "Scale to fit" option in image2cpp

### **Compilation error "array too large":**
- Your bitmap is too big
- Reduce image size or use lower resolution

---

## 📐 Positioning Options

### **Centered (Current):**
```cpp
int logoX = (SCREEN_WIDTH - LOGO_WIDTH) / 2;
int logoY = (SCREEN_HEIGHT - LOGO_HEIGHT) / 2;
```

### **Top Left:**
```cpp
int logoX = 5;  // 5 pixels from left
int logoY = 5;  // 5 pixels from top
```

### **Top Center:**
```cpp
int logoX = (SCREEN_WIDTH - LOGO_WIDTH) / 2;
int logoY = 5;
```

### **Bottom Right:**
```cpp
int logoX = SCREEN_WIDTH - LOGO_WIDTH - 5;
int logoY = SCREEN_HEIGHT - LOGO_HEIGHT - 5;
```

---

## 🎯 Quick Checklist

- [ ] Logo image prepared (64x32px recommended)
- [ ] Converted to bitmap array using image2cpp
- [ ] Copied array to OLED32.ino (line ~60)
- [ ] Updated LOGO_WIDTH and LOGO_HEIGHT
- [ ] Uploaded to XIAO
- [ ] Tested - logo appears on display 7!

---

## 📝 Current Setup Summary

**File:** `/Users/test/Music/TDS-8/OLED32/OLED32.ino`

**Bitmap location:** Lines 57-68
**Display function:** `showStartupSplash()` (line ~1334)
**Display:** Logo shows on display 7 (rightmost)
**Duration:** 5 seconds
**Position:** Centered

**To update:**
1. Replace bitmap array (line 60-64)
2. Update LOGO_WIDTH/HEIGHT (line 67-68)
3. Upload!

---

## 🔗 Useful Resources

- **image2cpp:** https://javl.github.io/image2cpp/
- **Adafruit GFX Graphics Library:** https://learn.adafruit.com/adafruit-gfx-graphics-library
- **OLED Display Tutorial:** https://randomnerdtutorials.com/esp32-ssd1306-oled-display-arduino-ide/

---

**Last Updated:** October 14, 2025  
**Version:** 0.32  
**Status:** ✅ Ready for your logo!
