# Fix "Sketch Too Big" Error on Mac

## The Problem
Mac Arduino IDE produces larger binaries than Windows for the same code.
Your sketch: **1,317,564 bytes** (6.8KB over the 1,310,720 limit)

## Solutions (Try in Order)

### Solution 1: Enable Compiler Optimizations ✅ **EASIEST**

In Arduino IDE:
1. **Tools → Optimize → "Optimize for Size (-Os)"**
   - This should save 50-100KB instantly

If that option doesn't exist, add to your sketch (top, after includes):

```cpp
#pragma GCC optimize ("Os")  // Optimize for size
```

### Solution 2: Partition Scheme Change 🔧 **VERY EFFECTIVE**

In Arduino IDE:
1. **Tools → Partition Scheme → "Huge APP (3MB No OTA/1MB SPIFFS)"**
   - This gives you 3MB for code instead of 1.25MB
   - Trade-off: No OTA updates (but you can add back later)

OR:

2. **Tools → Partition Scheme → "Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"**
   - Keeps OTA, gives you 1.9MB for code
   - Reduces SPIFFS storage

### Solution 3: Reduce HTML Size 📦 **IF ABOVE DON'T WORK**

The HTML in `handleRoot()` is ~6-8KB. Minify it more aggressively:

**Before** (readable):
```cpp
String html = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>TDS-8 Network</title>
<style>
:root{--brand:#ff8800;--brand2:#8a2be2}
// ... etc
)rawliteral";
```

**After** (ultra-minified):
```cpp
String html = R"(<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1"><title>TDS-8</title><style>:root{--brand:#f80;--brand2:#82e}*{box-sizing:border-box}body{margin:0;background:#f6f7fb;font:15px/1.4 system-ui}...)";
```

**Savings**: 1-2KB

### Solution 4: Serve HTML from SPIFFS 💾 **ADVANCED**

Instead of storing HTML in PROGMEM, store it in SPIFFS filesystem:

```cpp
void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  server.streamFile(file, "text/html");
  file.close();
}
```

Upload HTML separately using Arduino IDE → Tools → ESP32 Sketch Data Upload

**Savings**: 6-8KB from program memory

### Solution 5: Match PC's ESP32 Core Version 🔄 **GUARANTEED**

Your PC compiles smaller binaries - match its configuration:

1. On **PC**, check: Tools → Board → Boards Manager → "esp32" → note version
2. On **Mac**, install the SAME version
3. Restart Arduino IDE

Likely versions:
- **PC**: esp32 2.0.x (smaller binaries)
- **Mac**: esp32 3.0.x (larger binaries)

### Solution 6: Remove Unused Libraries 🗑️ **LAST RESORT**

If you're not using certain features, comment out:

```cpp
// #include <HTTPUpdate.h>  // If not using OTA
// #include <WiFiClientSecure.h>  // If not using HTTPS
// #include <ArduinoJson.h>  // If not using JSON (but you are)
```

## Recommended Approach

**Try this order**:

1. ✅ **Partition Scheme** → "Minimal SPIFFS (1.9MB APP with OTA)" 
   - Instant fix, keeps OTA
   
2. ✅ **Optimize** → "Optimize for Size (-Os)"
   - Extra insurance
   
3. ✅ **Match ESP32 Core Version** with your PC
   - Long-term consistency

This should get you **600KB+ headroom** for future development.

## Quick Test

After applying fixes, check the compile output:
```
Sketch uses XXXXX bytes (XX%) of program storage space. Maximum is XXXXXXX bytes.
```

Target: **< 80%** usage for comfortable development

## Why This Happens

- Different compiler versions on Mac vs Windows
- Different optimization defaults
- Different ESP32 Arduino Core versions
- Mac toolchain may be newer with more safety checks (larger code)
