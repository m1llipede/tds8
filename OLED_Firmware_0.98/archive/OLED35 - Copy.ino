/**********************************************************************
  TDS-8 (Track Display Strip - 8 channels)  — v35 WIFI & MULTI-DEVICE
  
  NEW IN V35:
  - WiFi IP updates in bridge dashboard
  - Dual /reannounce for reliability
  - Track numbers always shown at bottom
  - Removed false "Ableton Disconnected" messages
  - Track offset support (show tracks 1-8, 9-16, 17-24, etc.)
  - Display actual track numbers at bottom of OLEDs
  - 3-parameter /trackname command: <idx> <name> <actualTrack>
  - Blank displays on startup (no default "Track N" text)
  
  FEATURES:
  - Defaults to WIRED MODE (USB-C) - no WiFi required
  - Serial commands for full control via desktop bridge
  - Optional WiFi mode (enable via serial/web command)
  - Consumer-friendly plug-and-play experience
  - GitHub OTA firmware updates
  - 8x SSD1306 via TCA9548A (manual tcaSelect)
  - Serial: /trackname, /activetrack, VERSION, WIRED_ONLY, WIFI_ON, etc.
  - OSC (WiFi mode): /trackname, /activetrack, /reannounce
  - Web UI (WiFi mode): http://tds8.local
  - OTA updates from GitHub releases
**********************************************************************/

// =======================  Includes  =======================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

// =======================  Firmware  =======================
#define FW_VERSION "0.351"   // ← bump this when you publish a new firmware
#define FW_BUILD "2025.10.21-multi-device"  // ← build identifier (date + feature)
#define GITHUB_MANIFEST_URL "https://raw.githubusercontent.com/m1llipede/tds8/main/manifest.json"

// =======================  OLED / I2C  ======================
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET     -1
#define I2C_ADDR        0x3C
#define TCA_ADDRESS     0x70

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =======================  State Machine for Startup =======================
enum AppState {
  STATE_STARTUP_SPLASH,
  STATE_INSTRUCTIONS,
  STATE_RUNNING
};
AppState currentState = STATE_STARTUP_SPLASH;
unsigned long stateTransitionTime = 0;

// =======================  Graphics / Bitmaps  ==============
void tcaSelect(uint8_t i); // forward declaration
// Show TDS-8 and Playoptix logo splash on all screens for 3 seconds
void showLogosSplash() {
  for (uint8_t i = 0; i < 8; ++i) {
    tcaSelect(i);
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 8);
    display.println("TDS-8");
    display.setTextSize(1);
    display.setCursor(10, 40);
    display.println("by Playoptix");
    display.display();
  }
  delay(3000);
}

// PlayOptix logo - 128x64px (full display)
const unsigned char playoptix_logo [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xfc, 0x30, 0x00, 0x00, 0x00,
  0x78, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff,
  0x30, 0x00, 0x00, 0x01, 0xfe, 0x00, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x07, 0x07, 0x30, 0x00, 0x00, 0x03, 0x87, 0x00, 0x01, 0x80,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0xb0, 0x00, 0x00, 0x07,
  0x03, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x01,
  0xb0, 0xf8, 0x60, 0x6e, 0x01, 0xc3, 0xe1, 0xf2, 0x60, 0xc0, 0x00, 0x00,
  0x00, 0x00, 0x07, 0x01, 0xb1, 0xfc, 0x60, 0x6c, 0x00, 0xcf, 0xf1, 0xf6,
  0x70, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x07, 0x01, 0xb3, 0x86, 0x60, 0x4c,
  0x00, 0xcc, 0x39, 0x86, 0x39, 0x80, 0x00, 0x00, 0x00, 0x00, 0x06, 0x07,
  0x33, 0x06, 0x60, 0x4c, 0x00, 0xdc, 0x19, 0x86, 0x1b, 0x80, 0x00, 0x00,
  0x00, 0x00, 0x06, 0xff, 0x36, 0x03, 0x60, 0x6c, 0x00, 0xd8, 0x0d, 0x86,
  0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x7c, 0x36, 0x03, 0x60, 0x6c,
  0x01, 0xd8, 0x0d, 0x86, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00,
  0x33, 0x03, 0x60, 0xe6, 0x01, 0x98, 0x19, 0x86, 0x1d, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x07, 0x00, 0x33, 0x03, 0x70, 0xe7, 0x03, 0x98, 0x19, 0xc6,
  0x39, 0x80, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x33, 0xfb, 0x3f, 0xe3,
  0xff, 0x1b, 0xf0, 0xff, 0xf1, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00,
  0x30, 0xfb, 0x1f, 0xe1, 0xfc, 0x1b, 0xe0, 0x7f, 0xe0, 0xe0, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x60, 0x00, 0x18, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
  0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xc0, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x18, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x80,
  0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x1e, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

#define LOGO_WIDTH  128
#define LOGO_HEIGHT 64

// =======================  Tracks / UI  =====================
const int numScreens = 8;
String trackNames[numScreens] = {"", "", "", "", "", "", "", ""}; // Start blank
int actualTrackNumbers[numScreens] = {0, 1, 2, 3, 4, 5, 6, 7}; // Actual track numbers for offset support
int activeTrack = -1;
bool abletonConnected = false;
bool showingDisconnectMessage = false;
unsigned long heartbeatFlashUntil = 0;  // Show heartbeat dot until this time

// ==================  WIRED MODE (NEW)  ====================
bool wiredOnly = true;  // DEFAULT: wired mode, WiFi OFF
bool wifiEnabled = false;

// ==================  MULTI-DEVICE SUPPORT  ====================
uint8_t deviceID = 0;  // Device ID (0-3), determines track offset
// Device 0 = tracks 0-7 (displayed as 1-8)
// Device 1 = tracks 8-15 (displayed as 9-16)
// Device 2 = tracks 16-23 (displayed as 17-24)
// Device 3 = tracks 24-31 (displayed as 25-32)

// ==================  Network / services  ==================
Preferences prefs;
WebServer    server(80);
WiFiUDP      Udp;

const uint16_t oscPort         = 8000;    // OSC listener
const uint16_t ipBroadcastPort = 9000;    // broadcast lease + /ipupdate
const char*   mdnsName         = "tds8";  // http://tds8.local/

// Captive-portal AP creds
const char* AP_SSID = "TDS8";
const char* AP_PW   = "tds8setup";

// Optional static IP defaults (kept but DHCP-first behavior)
IPAddress currentIP  (192,168, 1,180);
IPAddress currentGW  (192,168, 1,  1);
IPAddress currentNM  (255,255,255,  0);
IPAddress currentDNS1(  8,  8,  8,  8);
IPAddress currentDNS2(  8,  8,  4,  4);

// Saved user Wi-Fi creds (web-form saves these)
String wifiSSID = "";
String wifiPW   = "";

// --- Rescue AP (fallback) ---
const char* RESCUE_SSID = "TDS8-Setup";
const char* RESCUE_PW   = "tds8setup";   // can be "" for open AP
bool        rescueAP     = false;
bool        rescueStopAt = 0;          // when to auto-stop rescue AP
const unsigned long RESCUE_AP_MAX_MS = 5UL * 60UL * 1000UL; // 5 minutes-5UL = 5m


// Discovery beacons after join (for hosts listening on 9000)
bool          discoveryActive   = false;
unsigned long discoveryUntil    = 0;
unsigned long nextBeaconAt      = 0;
const unsigned long beaconPeriodMs = 5000;
const unsigned long beaconWindowMs = 60000;

// UI state tracking
bool          abletonBannerShown = false;
bool          quickStartShown    = false;
volatile bool wantAbletonBanner = false;

// ===================  Forward decls  ======================
void tcaSelect(uint8_t i);
void drawTrackName(uint8_t screen, const String& name);
void refreshAll();
void showNetworkSetup();
void showNetworkSplash(const String& ip);
void showQuickStartLoop();
void showAbletonConnectedAll(uint16_t ms = 1000);
void showQuickStartInstructions();

void handleTrackName(OSCMessage &msg);
void handleActiveTrack(OSCMessage &msg);
void handleReannounceOSC(OSCMessage &msg);
void handleHi(OSCMessage &msg);
void sendHelloToM4L();
void handleTracksReset();

void handleRoot();
void handleStatus();
void handleScan();
void handleSave();
void handleReset();
void handleReannounceHTTP();
void handleWifiSave();
void handleForgetWifi();
void handleTracksGet();
void handleTracksPost();
void handleUpdate();  // OTA update

void saveTrackNames();
void loadTrackNames();

void saveConfig();
void loadConfig();
void saveWifiCreds();
void loadWifiCreds();
void broadcastIP();
void forgetWifi();

static uint16_t textWidth(const String& s, uint8_t ts);
void drawCentered(const String& s, uint8_t textSize, int y);
void drawLines(uint8_t ts, int y0, const char* l1=nullptr, const char* l2=nullptr, const char* l3=nullptr);

// NEW: Serial command handler
void handleSerialLine(const String& line);
void saveWiredMode();
void loadWiredMode();
void saveDeviceID();
void loadDeviceID();
void showStartupSplash();

// Bring up a temporary Access Point alongside station mode
void startRescueAP() {
  // Keep STA if already trying/connected; enable AP in dual mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(RESCUE_SSID, RESCUE_PW);
  rescueAP = true;
  rescueStopAt = millis() + RESCUE_AP_MAX_MS;

  IPAddress apip = WiFi.softAPIP();
  Serial.printf("🛟 Rescue AP up → SSID:%s PW:%s  AP-IP:%s\n",
                RESCUE_SSID, RESCUE_PW, apip.toString().c_str());
}

// Stop the temporary AP (go back to STA-only if desired)
void stopRescueAP() {
  if (!rescueAP) return;
  WiFi.softAPdisconnect(true);
  // If you want STA-only after stopping AP:
  WiFi.mode(WIFI_STA);
  rescueAP = false;
  Serial.println("🛟 Rescue AP stopped.");
}



// ========================  SETUP  =========================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Wire.begin();
  Wire.setClock(400000); // Fast I2C
  Wire.setTimeOut(15);   // ms

  Serial.println("\n\n=== TDS-8 v0.343 OFFSET SUPPORT ===");

  // Init all OLEDs
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR)) {
      Serial.printf("❌ OLED init failed on channel %d\n", i);
    } else {
      display.clearDisplay();
      display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
      display.display();
    }
  }

  // Load config + Wi-Fi creds + persisted track names + wired mode
  loadConfig();
  loadWifiCreds();
  loadTrackNames();
  // Load saved wired mode preference (defaults to true if never set)
  loadWiredMode();
  // Load saved device ID (defaults to 0 if never set)
  loadDeviceID();
  
  // Initialize actual track numbers based on device ID
  int offset = deviceID * 8;
  for (int i = 0; i < numScreens; i++) {
    actualTrackNumbers[i] = offset + i;
  }
  
  // Send mode status - if WiFi mode and already connected, include IP
  if (wiredOnly) {
    Serial.println("MODE: WIRED");
  } else {
    Serial.println("MODE: WIFI");
  }

  Serial.printf("VERSION: %s\n", FW_VERSION);
  Serial.printf("BUILD: %s\n", FW_BUILD);
  Serial.printf("WIRED_ONLY: %s\n", wiredOnly ? "true" : "false");
  Serial.printf("DEVICE_ID: %d\n", deviceID);
  
  // Kick off the non-blocking startup sequence
  showStartupSplash(); // This now only draws the logos, non-blockingly
  
  // ========== WIRED MODE (DEFAULT) ==========
  if (wiredOnly) {
    Serial.println("🔌 WIRED MODE - WiFi disabled");
    Serial.println("💡 Use desktop bridge or send 'WIFI_ON' to enable WiFi");
    
    WiFi.mode(WIFI_OFF);  // Explicitly turn off WiFi
    
    // In wired mode, skip instructions and go straight to running
    currentState = STATE_RUNNING;
    
    // Clear splash and show blank track displays
    refreshAll();
    Serial.println("🚀 Wired mode active - ready for serial commands");
    
    // No HTTP server, no OSC, no WiFi - just serial
    // Ready for serial commands
    return;  // Skip all WiFi setup
  }
  
  // WiFi mode: show splash then instructions
  currentState = STATE_STARTUP_SPLASH;
  stateTransitionTime = millis() + 3000; // Hold splash for 3 seconds
  // ========== WIFI MODE ==========
  
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Try to connect with saved creds
  if (wifiSSID.length()) WiFi.begin(wifiSSID.c_str(), wifiPW.c_str());
  else                   WiFi.begin();

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) delay(250);

  // If not connected -> setup or rescue
  if (WiFi.status() != WL_CONNECTED) {
    bool haveSaved = wifiSSID.length() > 0;
    WiFi.persistent(true);
    WiFi.mode(haveSaved ? WIFI_STA : WIFI_AP_STA);
    WiFi.disconnect(false, false);
    WiFi.softAPdisconnect(true);

    if (haveSaved) {
      Serial.println("⏳ Retrying STA join with saved creds…");
      WiFi.begin(wifiSSID.c_str(), wifiPW.c_str());
      unsigned long t1 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t1 < 20000UL) delay(250);

      if (WiFi.status() != WL_CONNECTED) {
        showNetworkSetup();
        Serial.println("⚠️ Still not connected; starting Rescue AP…");
        startRescueAP();
      }
    } else {
      showNetworkSetup();
      WiFiManager wm;
      wm.setConfigPortalBlocking(true);
      wm.setConfigPortalTimeout(180);
      bool ok = wm.startConfigPortal(AP_SSID, AP_PW);
      if (!ok) {
        WiFi.softAP(AP_SSID, AP_PW);
        IPAddress apip = WiFi.softAPIP();
        Serial.printf("⚠️ Portal timed out; AP up: %s pw:%s IP:%s\n",
                      AP_SSID, AP_PW, apip.toString().c_str());
      }
      if (!WiFi.softAPgetStationNum()) startRescueAP();
    }
  }

  // Connected?
  if (WiFi.status() == WL_CONNECTED) {
    // WiFi connected
    showNetworkSplash(WiFi.localIP().toString());
    startRescueAP();  // Keep rescue AP for a window
  } else {
    // WiFi not connected
    if (!WiFi.softAPgetStationNum()) startRescueAP();
  }

  // mDNS + HTTP
  MDNS.begin(mdnsName);

  // HTTP routes
  server.on("/",             HTTP_GET,  handleRoot);
  server.on("/status",       HTTP_GET,  handleStatus);
  server.on("/scan",         HTTP_GET,  handleScan);
  server.on("/save",         HTTP_POST, handleSave);
  server.on("/reset",        HTTP_POST, handleReset);
  server.on("/reannounce",   HTTP_POST, handleReannounceHTTP);
  server.on("/wifi",         HTTP_POST, handleWifiSave);
  server.on("/forget",       HTTP_POST, handleForgetWifi);
  server.on("/tracks/reset", HTTP_POST, handleTracksReset);
  server.on("/tracks",       HTTP_GET,  handleTracksGet);
  server.on("/tracks",       HTTP_POST, handleTracksPost);
  server.on("/update",       HTTP_POST, handleUpdate);

  server.begin();
  Serial.println("🌐 HTTP server started");

  // OSC
  Udp.begin(oscPort);
  Serial.printf("🎧 OSC listening on UDP %u\n", oscPort);

  broadcastIP();

  // Discovery beacons
  discoveryActive  = true;
  discoveryUntil   = millis() + beaconWindowMs;
  nextBeaconAt     = millis() + beaconPeriodMs;

  if (WiFi.status() == WL_CONNECTED) {
    showQuickStartLoop();
  }
}

// =========================  LOOP  =========================
static String serialBuffer = "";

void loop() {
  // ================== STATE MACHINE FOR STARTUP SEQUENCE ==================
  switch (currentState) {
    case STATE_STARTUP_SPLASH:
      if (millis() >= stateTransitionTime) {
        Serial.println("⏱️ Splash screen complete. Showing logos for 3s...");
        showLogosSplash();
        stateTransitionTime = millis() + 3000; // Hold logos for 3 seconds
        currentState = STATE_INSTRUCTIONS;
      }
      return; // Do nothing else during splash

    case STATE_INSTRUCTIONS:
      if (millis() >= stateTransitionTime) {
        Serial.println("✅ Logo splash complete. Switching to track displays.");
        refreshAll(); // Show blank track names now
        currentState = STATE_RUNNING;
      }
      return; // Do nothing else during instructions

    case STATE_RUNNING:
      break; // Fall through to normal operation
  }

  // ================== NORMAL OPERATION (after startup) ==================
  
  // ========== SERIAL COMMAND HANDLER (WORKS IN BOTH MODES) ==========
  static String serialBuffer = "";
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (serialBuffer.length()) {
        handleSerialLine(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
      if (serialBuffer.length() > 512) serialBuffer.remove(0, 1);
    }
  }

  // ========== WIFI MODE ONLY: Handle server, OSC, etc. ==========
  if (!wiredOnly && WiFi.getMode() != WIFI_OFF) {
    server.handleClient();

    // Rescue AP auto-stop
    if (rescueAP) {
      if (WiFi.status() == WL_CONNECTED) {
        WiFi.softAPdisconnect(true);
        rescueAP = false;
        // Rescue AP stopped - WiFi connected
        
        // Wait a moment for bridge to be ready, then broadcast IP multiple times
        delay(1000);
        broadcastIP();
        delay(500);
        broadcastIP();
        delay(500);
        broadcastIP();
        
        // Bridge will send /reannounce to M4L when it receives /ipupdate
        // No need to send it from device - avoids conflicts with handshake
      } else if (RESCUE_AP_MAX_MS > 0 && (long)(millis() - rescueStopAt) >= 0) {
        WiFi.softAPdisconnect(true);
        rescueAP = false;
        // Rescue AP stopped
        broadcastIP();
      }
    }

    // Ableton banner
    if (wantAbletonBanner) {
      wantAbletonBanner = false;
      showAbletonConnectedAll();
      refreshAll();
    }

    // OSC receive
    OSCMessage msg;
    int size;
    while ((size = Udp.parsePacket()) > 0) {
      while (size--) msg.fill(Udp.read());
      if (!msg.hasError()) {
        msg.dispatch("/hi",          handleHi);
        msg.dispatch("/trackname",   handleTrackName);
        msg.dispatch("/activetrack", handleActiveTrack);
        msg.dispatch("/reannounce",  handleReannounceOSC);
      }
      msg.empty();
      discoveryActive = false;
      yield();
    }

    // Discovery beacons and M4L heartbeat
    unsigned long now = millis();
    if (discoveryActive) {
      if (now >= discoveryUntil) {
        discoveryActive = false;
      } else if (now >= nextBeaconAt) {
        broadcastIP();
        sendHelloToM4L(); // Send /hello to M4L for heartbeat
        nextBeaconAt += beaconPeriodMs;
      }
    } else {
      // Keep sending /hello even after discovery period ends
      if (now >= nextBeaconAt) {
        sendHelloToM4L();
        nextBeaconAt = now + beaconPeriodMs;
      }
    }

    // Quick-Start once
    if (!quickStartShown && WiFi.status() == WL_CONNECTED && !abletonBannerShown) {
      showQuickStartLoop();
      quickStartShown = true;
    }
  }
  
  // ========== WIRED MODE ==========
  else {
  }
}

// ===============  OLED & TCA9548A helpers  ===============
void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCA_ADDRESS);
  Wire.write(1 << i);
  Wire.endTransmission(true);
}

// measure width at given text size
static uint16_t textWidth(const String& s, uint8_t ts) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(ts);
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void drawTrackName(uint8_t screen, const String& name) {
  tcaSelect(screen);
  display.invertDisplay(screen == activeTrack);
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);

  // Border
  display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

  String raw = name; raw.trim();

  const int MARGIN = 3, GAP = 2, maxW = SCREEN_WIDTH - 2 * MARGIN;
  const int CHAR_H = 8, CHAR_W = 6;

  auto _textWidth = [&](const String& s, uint8_t ts)->uint16_t{
    int16_t x1,y1; uint16_t w,h; display.setTextSize(ts);
    display.getTextBounds(s,0,0,&x1,&y1,&w,&h); return w;
  };
  auto drawSingleLine = [&](const String& line, uint8_t ts){
    display.setTextSize(ts);
    int w = _textWidth(line, ts);
    int lineH = CHAR_H * ts;
    int x = MARGIN + (maxW - w) / 2;
    int y = (SCREEN_HEIGHT - lineH) / 2;
    display.setCursor(x, y);
    display.println(line);
  };
  auto drawTwoLines = [&](const String& l1, const String& l2, uint8_t ts){
    display.setTextSize(ts);
    int w1 = _textWidth(l1, ts), w2 = _textWidth(l2, ts);
    int lineH = CHAR_H * ts, totalH = lineH * 2 + GAP;
    int y0 = (SCREEN_HEIGHT - totalH) / 2;
    int x1 = MARGIN + (maxW - w1) / 2;
    display.setCursor(x1, y0); display.println(l1);
    int x2 = MARGIN + (maxW - w2) / 2;
    display.setCursor(x2, y0 + lineH + GAP); display.println(l2);
  };
  auto trimToWidth = [&](String &line, uint8_t ts){
    int maxChars = max(1, maxW / (CHAR_W * ts));
    if ((int)line.length() > maxChars) {
      if (maxChars >= 3) line = line.substring(0, maxChars - 3) + "...";
      else               line = line.substring(0, maxChars);
    }
  };

  if (raw.length()) {
    // normalize spaces
    String s; s.reserve(raw.length()); bool lastSpace = false;
    for (size_t i=0;i<raw.length();++i){ char c=raw[i];
      if (c==' '||c=='\t'){ if(!lastSpace){ s+=' '; lastSpace=true; } }
      else { s+=c; lastSpace=false; } }
    while (s.length() && s[0]==' ') s.remove(0,1);
    while (s.length() && s[s.length()-1]==' ') s.remove(s.length()-1);

    bool drawn=false;
    // Try text size 2, then 1
    for (uint8_t ts : { (uint8_t)2, (uint8_t)1 }) {
      // First try: fit everything on one line
      if (_textWidth(s, ts) <= maxW) {
        drawSingleLine(s, ts);
        drawn=true;
        break;
      }
      
      // Second try: split into two lines by character count, prefer space break
      int maxChars = max(1, maxW/(CHAR_W*ts));
      if ((int)s.length() <= maxChars * 2) {
        // Find best break point (prefer space near middle)
        int idealBreak = s.length() / 2;
        int breakPoint = idealBreak;
        
        // Look for space within ±3 chars of ideal break
        for (int offset = 0; offset <= 3 && idealBreak + offset < (int)s.length(); offset++) {
          if (s[idealBreak + offset] == ' ') { breakPoint = idealBreak + offset; break; }
          if (idealBreak - offset >= 0 && s[idealBreak - offset] == ' ') { breakPoint = idealBreak - offset; break; }
        }
        
        String l1 = s.substring(0, breakPoint);
        String l2 = s.substring(breakPoint);
        l1.trim(); l2.trim();
        
        // Truncate if still too long
        trimToWidth(l1, ts);
        trimToWidth(l2, ts);
        
        if (_textWidth(l1, ts) <= maxW && _textWidth(l2, ts) <= maxW) {
          drawTwoLines(l1, l2, ts);
          drawn=true;
          break;
        }
      }
    }
    
    // Fallback: truncate to one line at size 1
    if (!drawn) {
      uint8_t ts=1;
      String l=s;
      trimToWidth(l, ts);
      drawSingleLine(l, ts);
    }
  }

  // Draw actual track number at bottom (small text)
  display.setTextSize(1);
  String trackNumStr = "Track " + String(actualTrackNumbers[screen] + 1); // "Track 1", "Track 2", etc.
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(trackNumStr, 0, 0, &x1, &y1, &w, &h);
  int trackNumX = (SCREEN_WIDTH - w) / 2;
  int trackNumY = SCREEN_HEIGHT - h - 3; // 3 pixels from bottom
  display.setCursor(trackNumX, trackNumY);
  display.println(trackNumStr);

  display.display();
  delay(0); // keep Wi-Fi/UDP breathing
}

void refreshAll() {
  for (uint8_t i = 0; i < numScreens; i++) drawTrackName(i, "");
} // Show blank center, only bottom track number, until names are set

void drawCentered(const String& s, uint8_t textSize, int y) {
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.println(s);
}

void drawLines(uint8_t ts, int y0, const char* l1, const char* l2, const char* l3) {
  const int CHAR_H = 8;
  int lineH = CHAR_H * ts;
  int y = y0;
  if (l1){ drawCentered(String(l1), ts, y); y += lineH + 2; }
  if (l2){ drawCentered(String(l2), ts, y); y += lineH + 2; }
  if (l3){ drawCentered(String(l3), ts, y); }
}

// =====================  Helper screens  ===================
void showNetworkSetup() {
  // WiFi setup screens removed - just show blank displays with borders
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    display.display();
  }
}

void showNetworkSplash(const String& ip) {
  const uint8_t i = 7;  // screen 8 (0-based)
  tcaSelect(i);
  display.invertDisplay(false);
  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

  uint8_t ts = 2;
  display.setTextSize(ts);

  int16_t x1, y1; uint16_t w1, h1, w2, h2;
  display.getTextBounds("TDS8",   0, 0, &x1, &y1, &w1, &h1);
  display.getTextBounds("ONLINE", 0, 0, &x1, &y1, &w2, &h2);

  const int maxW = SCREEN_WIDTH - 6;
  bool drewTitle = false;
  for (int gap = 2; gap >= 0; --gap) {
    int total = (int)w1 + gap + (int)w2;
    if (total <= maxW) {
      int x = (SCREEN_WIDTH - total) / 2;
      int y = 10;
      display.setCursor(x, y);                display.print("TDS8");
      display.setCursor(x + w1 + gap, y);     display.print("ONLINE");
      drewTitle = true;
      break;
    }
  }
  if (!drewTitle) {
    ts = 1;
    display.setTextSize(ts);
    drawCentered("TDS8 ONLINE", ts, 10);
  }

  drawCentered("IP: " + ip, 1, 36);
  display.display();
  delay(1500);
}

void showAbletonConnectedAll(uint16_t ms) {
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    drawLines(2, 15, "Ableton", "Connected", "");
    display.display();
    delay(0);
  }
  if (ms) delay(ms);
  refreshAll();
}

void showQuickStartLoop() {
  for (uint8_t i = 0; i < 7; ++i) {
    tcaSelect(i);
    display.invertDisplay(false);
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    switch (i % 8) {
      case 0: drawLines(2, 15, "Launch", "Ableton", ""); break;
      case 1: drawLines(2, 2,  "Drag", "TDS8.amxd", "to a track"); break;
      case 2: drawLines(2, 15, "Browser:", "tds8.local", ""); break;
      case 3: drawLines(2, 4,  "Hit", "Refresh", "Displays"); break;
      default: drawLines(2, 4,  "", "", ""); break;
    }
    display.display();
  }
  delay(500);
}

// ========================  OSC  ===========================
void handleTrackName(OSCMessage &msg) {
  if (msg.size() < 2) return;
  int idx = msg.getInt(0);
  char buf[64]; msg.getString(1, buf, sizeof(buf));
  int actualTrack = idx; // Default to display index
  
  // Check for optional 3rd parameter (actual track number)
  if (msg.size() >= 3) {
    actualTrack = msg.getInt(2);
  }
  
  if (idx >= 0 && idx < numScreens) {
    if (!abletonBannerShown) {
      abletonBannerShown = true;
      wantAbletonBanner  = true;
    }
    String incoming = String(buf);
    if (incoming.length() >= 2 &&
        incoming[0] == '\"' &&
        incoming[incoming.length()-1] == '\"') {
      incoming = incoming.substring(1, incoming.length()-1);
    }
    trackNames[idx] = incoming;
    actualTrackNumbers[idx] = actualTrack;
    drawTrackName(idx, trackNames[idx]);
    Serial.printf("RECV: /trackname %d '%s'\n", idx, buf);
  }
}

void handleActiveTrack(OSCMessage &msg) {
  if (msg.size() < 1) return;
  int idx = msg.getInt(0);
  if (idx < 0 || idx >= numScreens || idx == activeTrack) return;
  int old = activeTrack;
  activeTrack = idx;
  if (old >= 0) drawTrackName(old, trackNames[old]);
  drawTrackName(activeTrack, trackNames[activeTrack]);
  Serial.printf("RECV: /activetrack %d\n", idx);
}

void handleReannounceOSC(OSCMessage &msg) { broadcastIP(); }

void handleHi(OSCMessage &msg) {
  // Received /hi from M4L - Ableton is connected
  Serial.println("RECV: /hi");
  if (!abletonBannerShown) {
    showAbletonConnectedAll();
    abletonBannerShown = true;
  }
}

void sendHelloToM4L() {
  // Send /hello to M4L on port 9000 (broadcast)
  if (WiFi.status() != WL_CONNECTED) return;
  
  IPAddress gw = WiFi.gatewayIP();
  IPAddress bcast(gw[0], gw[1], gw[2], 255);
  
  OSCMessage msg("/hello");
  Udp.beginPacket(bcast, 9000);
  msg.send(Udp);
  Udp.endPacket();
  
  Serial.println("SENT: /hello");
}

// =====================  HTTP endpoints  ===================
void handleRoot() {
  // HTML page (same look; adds Firmware card)
  String html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>TDS-8 Network</title>
<style>
:root{--brand:#ff8800;--brand2:#8a2be2;--text:#222;--muted:#666;--card:#fff;--bg:#f6f7fb}
*{box-sizing:border-box}body{margin:0;background:var(--bg);font:15px/1.4 system-ui,Segoe UI,Roboto}
.wrap{max-width:820px;margin:32px auto;padding:0 16px}
.card{background:var(--card);border-radius:14px;box-shadow:0 10px 24px rgba(0,0,0,.08);overflow:hidden;margin-bottom:16px}
.hdr{background:linear-gradient(135deg,var(--brand),var(--brand2));padding:16px 20px;color:#fff}
.hdr h1{margin:0;font-size:20px;font-weight:600}
.body{padding:16px 20px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
label{display:block;color:var(--muted);font-size:12px;margin-bottom:4px}
.value,.input{padding:10px 12px;border:1px solid #e3e6ef;border-radius:10px;background:#fafbff}
.input{background:#fff;width:100%}
.actions{display:flex;gap:10px;margin-top:12px;flex-wrap:wrap}
.btn{appearance:none;border:0;border-radius:10px;padding:10px 14px;cursor:pointer}
.btn.primary{background:var(--brand);color:#fff}
.btn.ghost{background:#fff;border:1px solid #e3e6ef}
.note{margin-top:8px;color:var(--muted)}
.trackGrid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
.trackCard{border:1px solid #e3e6ef;border-radius:12px;padding:10px;background:#fff}
.trackCard h3{margin:0 0 6px 0;font-size:13px;color:#555}
.smallnote{margin-top:6px;color:#777;font-size:12px}
</style></head><body>
<div class="wrap">

  <div class="card">
    <div class="hdr"><h1>TDS-8 Network</h1></div>
    <div class="body">
      <div class="grid">
        <div><label>IP Address</label><div class="value" id="ip">–</div></div>
        <div><label>Gateway</label><div class="value" id="gw">–</div></div>
        <div><label>Subnet</label><div class="value" id="nm">–</div></div>
        <div><label>DNS 1</label><div class="value" id="dns1">–</div></div>
        <div><label>DNS 2</label><div class="value" id="dns2">–</div></div>
      </div>
      <div class="actions">
        <button class="btn primary" id="reannounce">Reannounce (broadcast IP)</button>
        <button class="btn ghost"   id="refresh">Refresh</button>
        <button class="btn ghost"   id="forget">Forget Wi-Fi</button>
      </div>
      <div class="smallnote">Firmware: <b id="fwver">FW_VERSION</b>. Use the Firmware card below to check for updates.</div>
      <div class="note" id="msg"></div>
    </div>
  </div>

  <div class="card">
    <div class="hdr"><h1>Firmware</h1></div>
    <div class="body">
      <div class="actions">
        <button class="btn primary" id="checkfw">Check for Update</button>
      </div>
      <div class="note" id="fwmsg"></div>
    </div>
  </div>

  <div class="card">
    <div class="hdr"><h1>Join a Different Wi-Fi</h1></div>
    <div class="body">
      <div class="grid">
        <div>
          <label>SSID <small>(type or pick)</small></label>
          <input class="input" list="ssidlist" id="ssid" placeholder="Network name"/>
          <datalist id="ssidlist"></datalist>
        </div>
        <div>
          <label>Password</label>
          <input class="input" id="pw" type="password" placeholder="••••••••"/>
        </div>
      </div>
      <div class="actions">
        <button class="btn primary" id="scan">Scan Networks</button>
        <button class="btn ghost"   id="connect">Connect & Save</button>
      </div>
      <div class="note">After connecting, the page may pause ~10–20s while the device switches networks.</div>
    </div>
  </div>

  <div class="card">
    <div class="hdr"><h1>Tracks Editor</h1></div>
    <div class="body">
      <p class="note">Type names for each display and click "Save Names" to push them live. These persist until Ableton overwrites via OSC.</p>
      <div id="trackGrid" class="trackGrid"></div>
      <div class="actions">
        <button class="btn primary" id="saveNames">Save Names</button>
        <button class="btn ghost" id="reloadNames">Reload From Device</button>
        <button class="btn ghost" id="resetNames">Reset Names to Defaults</button> 
      </div>
      <div class="note" id="tracksMsg"></div>
    </div>
  </div>

</div>

<!-- Scan modal -->
<div id="scanModal" class="modal hidden" style="position:fixed;inset:0;background:rgba(0,0,0,.45);display:none;align-items:center;justify-content:center;z-index:9999">
  <div class="modal-card" style="width:min(520px,90vw);background:#fff;border-radius:14px;box-shadow:0 18px 40px rgba(0,0,0,.25);overflow:hidden">
    <div class="modal-hdr" style="padding:14px 16px;background:linear-gradient(135deg,var(--brand),var(--brand2));color:#fff;font-weight:600">Available Wi-Fi Networks</div>
    <div class="modal-body" id="scanResults" style="max-height:60vh;overflow:auto;padding:12px 16px"><div class="note">Scanning…</div></div>
    <div class="modal-ftr" style="display:flex;justify-content:flex-end;gap:10px;padding:12px 16px;background:#fafbff"><button class="btn ghost" id="closeScan">Close</button></div>
  </div>
</div>

<script>
const F=id=>document.getElementById(id);

// ----- Status -----
async function loadStatus(){
  try{
    const r=await fetch('/status'); const j=await r.json();
    ['ip','gw','nm','dns1','dns2'].forEach(k=>{ const el=F(k); if(el) el.textContent=j[k]||'–'; });
  }catch(e){}
}
loadStatus(); setInterval(loadStatus, 2000);

async function post(url, body){
  try{
    const r = await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{})});
    return await r.text();
  }catch(e){ return 'error'; }
}

F('refresh').onclick = ()=> location.reload(true);
F('reannounce').onclick = async ()=>{ F('msg').textContent='Broadcasting…'; await post('/reannounce'); setTimeout(()=>F('msg').textContent='',3000); };

const modal = document.getElementById('scanModal');
const results = document.getElementById('scanResults');
document.getElementById('closeScan').onclick = ()=> { modal.style.display='none'; };

F('scan').onclick = async ()=>{
  results.innerHTML = '<div class="note">Scanning…</div>';
  modal.style.display='flex';
  try{
    const r = await fetch('/scan'); const arr = await r.json();
    if(!Array.isArray(arr) || arr.length===0){ results.innerHTML='<div class="note">No networks found.</div>'; return; }
    results.innerHTML = arr.map(o=>{
      const enc = (o.enc===7 || o.enc==='OPEN') ? 'Open' : 'Secured';
      const rssi = (o.rssi!==undefined) ? `${o.rssi} dBm` : '';
      const ssid = o.ssid || '(hidden)';
      return `<div class="net" data-ssid="${ssid.replace(/"/g,'&quot;')}" style="display:flex;align-items:center;justify-content:space-between;border:1px solid #e3e6ef;border-radius:10px;padding:8px 10px;margin:6px 0;cursor:pointer">
                <div class="ssid" style="font-weight:600">${ssid}</div>
                <div class="meta" style="color:#777;font-size:12px">${enc} ${rssi}</div>
              </div>`;
    }).join('');
    results.querySelectorAll('.net').forEach(el=>{
      el.onclick = ()=>{
        const ssid = el.dataset.ssid;
        if(ssid && ssid!=='(hidden)') F('ssid').value = ssid;
        modal.style.display='none'; F('pw').focus();
      };
    });
  }catch(e){ results.innerHTML='<div class="note">Scan failed.</div>'; }
};

F('connect').onclick = async ()=>{
  const ssid=F('ssid').value.trim(), pw=F('pw').value;
  if(!ssid){ F('msg').textContent='Enter SSID'; return; }
  F('msg').textContent='Connecting… device may pause for 10–20s';
  await post('/wifi',{ssid,pw}); setTimeout(()=>location.reload(), 12000);
};

F('forget').onclick = async ()=>{ F('msg').textContent = 'Forgetting Wi-Fi… device will reboot into setup'; await post('/forget'); };

// ----- Tracks Editor -----
const tgrid = document.getElementById('trackGrid');
const tmsg  = document.getElementById('tracksMsg');
let names = Array(8).fill('');

function renderTracks(){
  tgrid.innerHTML = '';
  for(let i=0;i<8;i++){
    const card = document.createElement('div'); card.className='trackCard';
    const h = document.createElement('h3'); h.textContent = `Track ${i+0}`;
    const inp = document.createElement('input'); inp.className='input'; inp.id='name'+i; inp.value = names[i] || `Track ${i+0}`;
    card.appendChild(h); card.appendChild(inp);
    tgrid.appendChild(card);
  }
}

async function loadTracks(){
  try{
    const r = await fetch('/tracks', { cache: 'no-store' });
    const j = await r.json();
    names = Array.isArray(j.names) && j.names.length===8 ? j.names : names;
  }catch(e){}
  renderTracks();
}
F('reloadNames').onclick = loadTracks;

F('saveNames').onclick = async ()=>{
  const out = [];
  for(let i=0;i<8;i++){ out.push(document.getElementById('name'+i).value); }
  tmsg.textContent = 'Saving...';
  const res = await post('/tracks',{names:out});
  tmsg.textContent = res.includes('"ok"') ? 'Names saved & pushed to displays.' : 'Save failed.';
  setTimeout(()=>tmsg.textContent='',2500);
};

F('resetNames').onclick = async ()=>{
  tmsg.textContent = 'Resetting...';
  const res = await post('/tracks/reset', {});
  if(res.includes('"ok"')){ await loadTracks(); tmsg.textContent = 'Names reset.'; }
  else { tmsg.textContent = 'Reset failed.'; }
  setTimeout(()=>tmsg.textContent='', 2000);
};

loadTracks();

// ----- Firmware -----
document.getElementById('fwver').textContent = 'FW_VERSION';

F('checkfw').onclick = async ()=>{
  const msg = F('fwmsg');
  msg.textContent = 'Checking…';
  try{
    const r = await fetch('GITHUB_MANIFEST_URL', {cache:'no-store'});
    const j = await r.json();
    if (!j || !j.version || !j.url){ msg.textContent='Bad manifest.'; return; }
    if (j.version !== 'FW_VERSION'){
      if (confirm(`New firmware ${j.version} available. Update now?`)) {
        msg.textContent = 'Updating… device will reboot after flash.';
        await fetch('/update', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(j)});
      } else {
        msg.textContent = 'Update canceled.';
      }
    } else {
      msg.textContent = 'Already up to date.';
    }
  }catch(e){
    msg.textContent = 'Check failed.';
  }
};
</script>
</body></html>
)rawliteral";

  html.replace("FW_VERSION", FW_VERSION);
  html.replace("GITHUB_MANIFEST_URL", GITHUB_MANIFEST_URL);
  server.send(200, "text/html", html);
}

void handleStatus() {
  DynamicJsonDocument doc(256);
  doc["ip"]   = WiFi.localIP().toString();
  doc["gw"]   = WiFi.gatewayIP().toString();
  doc["nm"]   = WiFi.subnetMask().toString();
  doc["dns1"] = WiFi.dnsIP(0).toString();
  doc["dns2"] = WiFi.dnsIP(1).toString();
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleScan() {
  WiFi.mode(WIFI_AP_STA);
  delay(200);
  int n = WiFi.scanNetworks(false, true);

  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["enc"]  = WiFi.encryptionType(i);  // 7=open on some cores
  }
  WiFi.scanDelete();
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// POST /save — configure static IP (optional) or DHCP if omitted
void handleSave() {
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"result\":\"error\",\"msg\":\"bad json\"}");
    return;
  }
  String ipS = doc["ip"]   | "";
  String gwS = doc["gw"]   | "";
  String nmS = doc["nm"]   | "";
  String d1S = doc["dns1"] | "";
  String d2S = doc["dns2"] | "";

  if (ipS.length()) currentIP.fromString(ipS);
  if (gwS.length()) currentGW.fromString(gwS);
  if (nmS.length()) currentNM.fromString(nmS);
  if (d1S.length()) currentDNS1.fromString(d1S);
  if (d2S.length()) currentDNS2.fromString(d2S);
  saveConfig();

  WiFi.disconnect(true);
  bool wantStatic = ipS.length() > 0;
  if (wantStatic) WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  else            WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

  WiFi.begin();
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(200);

  if (wantStatic && WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.begin();
    t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    broadcastIP();
    server.send(200,"application/json","{\"result\":\"ok\"}");
  } else {
    server.send(200,"application/json","{\"result\":\"fail\"}");
  }
}

void handleReset() {
  currentIP   = IPAddress(192,168,1,180);
  currentGW   = IPAddress(192,168,1,1);
  currentNM   = IPAddress(255,255,255,0);
  currentDNS1 = IPAddress(8,8,8,8);
  currentDNS2 = IPAddress(8,8,4,4);
  saveConfig();

  WiFi.disconnect(true);
  WiFi.config(currentIP, currentGW, currentNM, currentDNS1, currentDNS2);
  WiFi.begin(); while (WiFi.status() != WL_CONNECTED) delay(200);
  broadcastIP();
  server.send(200,"application/json","{\"result\":\"reset\"}");
}

void handleReannounceHTTP(){ broadcastIP(); server.send(200,"application/json","{\"result\":\"ok\"}"); }

// POST /wifi {ssid,pw} — save creds and reconnect
void handleWifiSave() {
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"bad json\"}");
    return;
  }
  String ssid = doc["ssid"] | "";
  String pw   = doc["pw"]   | "";
  if (!ssid.length()) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"missing ssid\"}");
    return;
  }

  wifiSSID = ssid; wifiPW = pw;
  saveWifiCreds();

  WiFi.disconnect(true);
  WiFi.begin(wifiSSID.c_str(), wifiPW.c_str());

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 15000) delay(250);

  if (WiFi.status() == WL_CONNECTED) {
    showNetworkSplash(WiFi.localIP().toString());
    broadcastIP();
    discoveryActive = true;
    discoveryUntil  = millis() + beaconWindowMs;
    nextBeaconAt    = millis() + beaconPeriodMs;
    server.send(200,"application/json","{\"result\":\"ok\"}");
  } else {
    server.send(200,"application/json","{\"result\":\"fail\"}");
  }
}

// ---------- Tracks endpoints + persistence ----------
void handleTracksGet() {
  DynamicJsonDocument doc(1024);
  JsonArray namesArr = doc.createNestedArray("names");
  for (int i=0;i<numScreens;i++) namesArr.add(trackNames[i]);
  String out; serializeJson(doc, out);

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.send(200, "application/json", out);
}

void handleTracksPost() {
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"bad json\"}");
    return;
  }
  if (!doc.containsKey("names") || !doc["names"].is<JsonArray>()) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"missing names[]\"}");
    return;
  }
  JsonArray arr = doc["names"].as<JsonArray>();
  for (uint8_t i=0;i<numScreens && i<arr.size();i++){
    trackNames[i] = String((const char*)arr[i]);
  }
  saveTrackNames();
  refreshAll();
  server.send(200,"application/json","{\"result\":\"ok\"}");
}

void handleTracksReset() {
  for (int i = 0; i < numScreens; i++) {
    trackNames[i] = "Track " + String(i + 1);
  }
  saveTrackNames();
  refreshAll();
  server.send(200, "application/json", "{\"result\":\"ok\"}");
}

// ---------- OTA update endpoint ----------
void handleUpdate() {
  // Body: {"version":"<ver>","url":"https://.../firmware.bin"}
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"bad json\"}");
    return;
  }
  String url = doc["url"] | "";
  if (!url.length()) {
    server.send(400,"application/json","{\"result\":\"error\",\"msg\":\"missing url\"}");
    return;
  }

  server.send(200,"application/json","{\"result\":\"ok\",\"msg\":\"updating\"}");
  delay(300);

  WiFiClientSecure client;
  client.setInsecure();                // accept GitHub/S3 certs without bundle
  httpUpdate.rebootOnUpdate(true);     // auto-reboot if update succeeds

  t_httpUpdate_return ret = httpUpdate.update(client, url);
  if (ret != HTTP_UPDATE_OK) {
    Serial.printf("OTA failed: %d %s\n", (int)ret, httpUpdate.getLastErrorString().c_str());
  }
}

// ==================  NVS save / load helpers  ==================
void saveConfig() {
  prefs.begin("net", false);
  prefs.putString("ip",   currentIP.toString());
  prefs.putString("gw",   currentGW.toString());
  prefs.putString("nm",   currentNM.toString());
  prefs.putString("dns1", currentDNS1.toString());
  prefs.putString("dns2", currentDNS2.toString());
  prefs.end();
}
void loadConfig() {
  prefs.begin("net", true);
  currentIP  .fromString(prefs.getString("ip",   currentIP.toString()));
  currentGW  .fromString(prefs.getString("gw",   currentGW.toString()));
  currentNM  .fromString(prefs.getString("nm",   currentNM.toString()));
  currentDNS1.fromString(prefs.getString("dns1", currentDNS1.toString()));
  currentDNS2.fromString(prefs.getString("dns2", currentDNS2.toString()));
  prefs.end();
}
void saveWifiCreds(){
  prefs.begin("wifi", false);
  prefs.putString("ssid", wifiSSID);
  prefs.putString("pw",   wifiPW);
  prefs.end();
}
void loadWifiCreds(){
  prefs.begin("wifi", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPW   = prefs.getString("pw",   "");
  prefs.end();
}

// Persist track names individually
void saveTrackNames(){
  prefs.begin("tracks", false);
  for (int i=0;i<numScreens;i++){
    String key = "name" + String(i);
    prefs.putString(key.c_str(), trackNames[i]);
  }
  prefs.end();
}
void loadTrackNames(){
  prefs.begin("tracks", true);
  for (int i=0;i<numScreens;i++){
    String key = "name" + String(i);
    String v = prefs.getString(key.c_str(), trackNames[i]);
    trackNames[i] = v;
  }
  prefs.end();
}

// ===================  Broadcast current IP  ===================
void broadcastIP() {
  if (WiFi.status() != WL_CONNECTED) return;

  IPAddress gw = WiFi.gatewayIP();
  IPAddress bcast(gw[0], gw[1], gw[2], 255);
  String s = WiFi.localIP().toString();

  Udp.beginPacket(bcast, ipBroadcastPort); Udp.print(s); Udp.endPacket();

  OSCMessage m("/ipupdate");
  m.add(s.c_str());
  Udp.beginPacket(bcast, ipBroadcastPort); m.send(Udp); Udp.endPacket();
  delay(0);
  Serial.printf("SENT: /ipupdate %s\n", s.c_str());
}

// ======================  Forget Wi-Fi  =======================
void handleForgetWifi() {
  WiFi.disconnect(true /*wifioff*/, true /*erasePersistent*/);
  WiFi.mode(WIFI_OFF);
  delay(200);

  { WiFiManager wm; wm.resetSettings(); }

  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  server.send(200, "application/json", "{\"result\":\"ok\"}");
  delay(500);
  ESP.restart();
}

void forgetWifi() {
  Serial.println("Forgetting Wi-Fi credentials and rebooting...");
  WiFi.disconnect(true /* wifioff */, true /* erasePersistent */);
  WiFi.mode(WIFI_OFF);
  delay(200);

  { WiFiManager wm; wm.resetSettings(); }

  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  delay(200);
  ESP.restart();
}

// ==================  WIRED MODE HELPERS (NEW)  ==================
void saveWiredMode() {
  prefs.begin("mode", false);
  prefs.putBool("wired", wiredOnly);
  prefs.end();
}

void loadWiredMode() {
  prefs.begin("mode", true);
  wiredOnly = prefs.getBool("wired", true);  // default true = wired
  prefs.end();
}

void saveDeviceID() {
  prefs.begin("device", false);
  prefs.putUChar("id", deviceID);
  prefs.end();
}

void loadDeviceID() {
  prefs.begin("device", true);
  deviceID = prefs.getUChar("id", 0);  // default 0 = first device
  if (deviceID > 3) deviceID = 0;  // Validate range 0-3
  prefs.end();
}

void showStartupSplash() {
  // Draws the initial TDS-8 Logo and PlayOptix logo without any delay.
  const char* letters[] = {"T", "D", "S", "-", "8"};
  
  // Clear ALL displays completely first
  for (uint8_t i = 0; i < numScreens; i++) {
    tcaSelect(i);
    display.clearDisplay();
    display.display();
    delay(5); // Small delay to ensure clear completes
  }
  
  // Displays 1-5: T D S - 8
  for (uint8_t i = 0; i < 5; i++) {
    tcaSelect(i + 1);
    display.invertDisplay(false);
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(6);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(letters[i], 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.println(letters[i]);
    display.display();
  }
  
  // Display 7: PlayOptix logo
  tcaSelect(7);
  display.invertDisplay(false);
  display.clearDisplay();
  display.display(); // Ensure clear is applied
  delay(10);
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);
  display.drawBitmap(0, 0, playoptix_logo, LOGO_WIDTH, LOGO_HEIGHT, SSD1306_WHITE);
  // Add a black rectangle at the bottom to cover any graphical artifacts
  display.fillRect(0, 54, 128, 10, SSD1306_BLACK);
  display.display();
}

void showQuickStartInstructions() {
  // Draws the instruction screens without any delay.
  int16_t x1, y1;
  uint16_t w, h;

  // Display 0
  tcaSelect(0);
  display.clearDisplay();
  display.setTextSize(2);
  display.getTextBounds("Launch", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 11);
  display.println("Launch");
  display.getTextBounds("the", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 28);
  display.println("the");
  display.getTextBounds("TDS-8", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 45);
  display.println("TDS-8");
  display.display();

  // Display 1
  tcaSelect(1);
  display.clearDisplay();
  display.setTextSize(2);
  display.getTextBounds("Hit", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 18);
  display.println("Hit");
  display.getTextBounds("Connect", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 36);
  display.println("Connect");
  display.display();

  // Display 2
  tcaSelect(2);
  display.clearDisplay();
  display.setTextSize(2);
  display.getTextBounds("Launch", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 18);
  display.println("Launch");
  display.getTextBounds("Ableton", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 36);
  display.println("Ableton");
  display.display();

  // Display 3
  tcaSelect(3);
  display.clearDisplay();
  display.setTextSize(2);
  display.getTextBounds("Drag", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 11);
  display.println("Drag");
  display.getTextBounds("TDS8", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 28);
  display.println("TDS8");
  display.getTextBounds("to a Track", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 45);
  display.println("to a Track");
  display.display();

  // Display 4
  tcaSelect(4);
  display.clearDisplay();
  display.setTextSize(2);
  display.getTextBounds("Tap Refresh", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 28);
  display.println("Tap Refresh");
  display.display();

  // Display 5
  tcaSelect(5);
  display.clearDisplay();
  display.setTextSize(2);
  display.getTextBounds("In Device", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 11);
  display.println("In Device");
  display.getTextBounds("or", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 28);
  display.println("or");
  display.getTextBounds("Dashboard", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 45);
  display.println("Dashboard");
  display.display();

  // Display 6 - blank
  tcaSelect(6);
  display.clearDisplay();
  display.display();

  // Display 7 (OLED 8) - Show PlayOptix logo
  tcaSelect(7);
  display.clearDisplay();
  display.display(); // Ensure clear is applied
  delay(10);
  display.drawBitmap(0, 0, playoptix_logo, LOGO_WIDTH, LOGO_HEIGHT, SSD1306_WHITE);
  display.fillRect(0, 54, 128, 10, SSD1306_BLACK); // Cover artifacts at bottom
  display.display();
}

// ==================  SERIAL COMMAND HANDLER (NEW)  ==================
void handleSerialLine(const String& line) {
  String cmd = line;
  cmd.trim();
  if (!cmd.length()) return;

  String head = cmd;
  int sp = cmd.indexOf(' ');
  if (sp >= 0) head = cmd.substring(0, sp);
  head.toLowerCase();

  // VERSION
  if (head == "version") {
    Serial.printf("VERSION: %s\n", FW_VERSION);
    Serial.printf("BUILD: %s\n", FW_BUILD);
    return;
  }

  // DEVICE_ID 0-3
  if (head == "device_id") {
    String arg = (sp >= 0) ? cmd.substring(sp + 1) : "";
    arg.trim();
    int newID = arg.toInt();
    
    if (newID < 0 || newID > 3) {
      Serial.println("ERR: DEVICE_ID must be 0-3");
      Serial.println("   Device 0 = tracks 1-8");
      Serial.println("   Device 1 = tracks 9-16");
      Serial.println("   Device 2 = tracks 17-24");
      Serial.println("   Device 3 = tracks 25-32");
      return;
    }
    
    // Always update track numbers and refresh display, even if ID hasn't changed
    deviceID = newID;
    saveDeviceID();
    
    // Update actual track numbers based on device ID
    int offset = deviceID * 8;
    for (int i = 0; i < numScreens; i++) {
      actualTrackNumbers[i] = offset + i;
    }
    
    // Force device to running state to synchronize with other devices
    // This ensures all devices show the same screen (track displays) in multi-device setups
    currentState = STATE_RUNNING;
    
    refreshAll();
    Serial.printf("OK: DEVICE_ID set to %d (tracks %d-%d)\n", deviceID, offset + 1, offset + 8);
    return;
  }

  // WIRED_ONLY true/false
  if (head == "wired_only") {
    String arg = (sp >= 0) ? cmd.substring(sp + 1) : "";
    arg.trim(); arg.toLowerCase();
    bool newMode = (arg == "true" || arg == "1");
    
    if (newMode != wiredOnly) {
      wiredOnly = newMode;
      saveWiredMode();
      Serial.printf("OK: WIRED_ONLY %s (reboot to apply)\n", wiredOnly ? "true" : "false");
      Serial.println("💡 Power cycle or send REBOOT to apply mode change");
    } else {
      Serial.printf("OK: WIRED_ONLY already %s\n", wiredOnly ? "true" : "false");
    }
    return;
  }

  // WIFI_ON (enable WiFi - requires reboot)
  if (head == "wifi_on" || head == "wifi") {
    if (wiredOnly) {
      Serial.println("ERR: In wired mode. Set WIRED_ONLY false first, then reboot.");
      Serial.println("💡 Commands: WIRED_ONLY false → REBOOT");
    } else {
      Serial.println("OK: Already in WiFi mode");
    }
    return;
  }

  // WIFI_JOIN "ssid" "password"
  if (head == "wifi_join") {
    int q1 = cmd.indexOf('"');
    int q2 = cmd.indexOf('"', q1 + 1);
    int q3 = cmd.indexOf('"', q2 + 1);
    int q4 = cmd.indexOf('"', q3 + 1);
    
    if (q1 >= 0 && q2 > q1 && q3 >= 0 && q4 > q3) {
      String ssid = cmd.substring(q1 + 1, q2);
      String pass = cmd.substring(q3 + 1, q4);
      
      wifiSSID = ssid;
      wifiPW = pass;
      saveWifiCreds();
      
      Serial.printf("OK: WiFi credentials saved (SSID: %s)\n", ssid.c_str());
      Serial.println("💡 Set WIRED_ONLY false and reboot to connect");
    } else {
      Serial.println("ERR: Format: WIFI_JOIN \"ssid\" \"password\"");
    }
    return;
  }

  // FORGET
  if (head == "forget") {
    wifiSSID = "";
    wifiPW = "";
    saveWifiCreds();
    Serial.println("OK: WiFi credentials cleared");
    return;
  }

  // REBOOT
  if (head == "reboot") {
    Serial.println("OK: Rebooting...");
    delay(500);
    ESP.restart();
    return;
  }

  // /trackname <idx> "name" [actualTrack]
  if (cmd.startsWith("/trackname")) {
    int sp1 = cmd.indexOf(' ');
    if (sp1 > 0) {
      int sp2 = cmd.indexOf(' ', sp1 + 1);
      if (sp2 > 0) {
        int idx = cmd.substring(sp1 + 1, sp2).toInt();
        
        // Find quoted name - look for opening quote
        int quoteStart = cmd.indexOf('"', sp2);
        if (quoteStart > 0) {
          // Find closing quote
          int quoteEnd = cmd.indexOf('"', quoteStart + 1);
          if (quoteEnd > 0) {
            // Extract name between quotes
            String name = cmd.substring(quoteStart + 1, quoteEnd);
            name.trim();
            
            // Look for actualTrack after closing quote
            int actualTrack = idx; // Default to display index
            int sp3 = cmd.indexOf(' ', quoteEnd + 1);
            if (sp3 > 0) {
              actualTrack = cmd.substring(sp3 + 1).toInt();
            }
            
            if (idx >= 0 && idx < numScreens) {
              if (!abletonBannerShown) {
                abletonBannerShown = true;
                if (!wiredOnly) wantAbletonBanner = true;
              }

              // If name is "Track", treat it as blank. Otherwise, use the provided name.
              String finalName = (name == "Track") ? "" : name;

              trackNames[idx] = finalName;
              actualTrackNumbers[idx] = actualTrack;
              drawTrackName(idx, trackNames[idx]);
              Serial.printf("OK: /trackname %d \"%s\" (track %d)\n", idx, finalName.c_str(), actualTrack);
            } else {
              Serial.println("ERR: Index out of range (0-7)");
            }
            return;
          }
        }
      }
    }
    Serial.println("ERR: Format: /trackname <idx> \"name\" [actualTrack]");
    return;
  }

  // /activetrack <idx>
  if (cmd.startsWith("/activetrack")) {
    int sp1 = cmd.indexOf(' ');
    if (sp1 > 0) {
      int idx = cmd.substring(sp1 + 1).toInt();
      if (idx >= 0 && idx < numScreens) {
        int old = activeTrack;
        activeTrack = idx;
        if (old >= 0) drawTrackName(old, trackNames[old]);
        drawTrackName(activeTrack, trackNames[activeTrack]);
        Serial.printf("OK: /activetrack %d\n", idx);
      } else {
        Serial.println("ERR: Index out of range (0-7)");
      }
      return;
    }
    Serial.println("ERR: Format: /activetrack <idx>");
    return;
  }

  // /ableton_on - Ableton is connected
  if (cmd.startsWith("/ableton_on")) {
    abletonConnected = true;
    heartbeatFlashUntil = millis() + 300;  // Flash heartbeat for 300ms
    if (showingDisconnectMessage) {
      showingDisconnectMessage = false;
      refreshAll(); // Redraw track names
    } else {
      // Just redraw OLED 8 to show heartbeat
      drawTrackName(7, trackNames[7]);
    }
    // Ableton connected
    return;
  }

  // /ableton_off - Ableton disconnected
  if (cmd.startsWith("/ableton_off")) {
    abletonConnected = false;
    // Don't show disconnect message - it causes false positives
    // Just update the connection flag
    return;
  }


  // /reannounce
  if (cmd.startsWith("/reannounce")) {
    if (!wiredOnly && WiFi.status() == WL_CONNECTED) {
      broadcastIP();
      Serial.println("SENT: /reannounce");
    } else {
      Serial.println("SENT: /reannounce");
    }
    return;
  }

  // Unknown command
  Serial.println("ERR: Unknown command");
  Serial.println("📋 Available commands:");
  Serial.println("   VERSION - Show firmware version");
  Serial.println("   DEVICE_ID 0-3 - Set device ID for multi-device setups");
  Serial.println("   WIRED_ONLY true/false - Toggle wired/WiFi mode");
  Serial.println("   WIFI_JOIN \"ssid\" \"password\" - Save WiFi credentials");
  Serial.println("   FORGET - Clear WiFi credentials");
  Serial.println("   REBOOT - Restart device");
  Serial.println("   /trackname <idx> <name> - Set track name (0-7)");
  Serial.println("   /activetrack <idx> - Highlight active track");
  Serial.println("   /reannounce - Broadcast IP (WiFi mode only)");
}
