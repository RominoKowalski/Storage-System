#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>   // für wifi_tx_info_t (optional, aber sicherheitshalber)

// ============ TOUCH PINS ============
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// ============ DISPLAY (Landscape) ============
#define SCREEN_W 320
#define SCREEN_H 240

// ============ TOUCH CALIBRATION ============
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3800

// ============ THEME COLORS ============
uint16_t BG_COLOR = TFT_BLACK;
uint16_t TEXT_COLOR = TFT_WHITE;
uint16_t PANEL_COLOR = 0x0841;
uint16_t HEADER_COLOR = TFT_DARKGREY;
uint16_t BORDER_COLOR = TFT_DARKGREY;

void applyTheme() {
  BG_COLOR = TFT_BLACK;
  TEXT_COLOR = TFT_WHITE;
  PANEL_COLOR = 0x1082;
  HEADER_COLOR = 0x2104;
  BORDER_COLOR = TFT_DARKGREY;
}

// ============ OBJECTS ============
TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

bool getTouch(int& x, int& y);

// ================= BUTTON =================
void drawButton(int x, int y, int w, int h, uint16_t color, String text) {
  tft.fillRoundRect(x, y, w, h, 8, color);
  tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, color);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(text, x + w / 2, y + h / 2, 2);
}

bool isButtonPressed(int tx, int ty, int x, int y, int w, int h) {
  return (tx >= x && tx <= x + w && ty >= y && ty <= y + h);
}

void drainTouch() {
  int _tx, _ty;
  while (getTouch(_tx, _ty)) delay(10);
}

// ================= TOUCH =================
bool getTouch(int& x, int& y) {
  if (!touchscreen.tirqTouched()) return false;
  if (!touchscreen.touched()) return false;
  TS_Point p = touchscreen.getPoint();
  x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);
  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);
  return true;
}

// ================= 3x3 GRID BUTTONS =================
struct AppButton {
  int x, y, w, h;
  uint16_t color;
  const char* label;
};

static const AppButton MENU_GRID[] = {
  // Reihe 1
  {  5, 25, 100, 65, TFT_BLUE,    "1" },
  {110, 25, 100, 65, TFT_RED,     "2" },
  {215, 25, 100, 65, TFT_GREEN,   "3" },
  // Reihe 2
  {  5, 95, 100, 65, TFT_CYAN,    "4" },
  {110, 95, 100, 65, TFT_MAGENTA, "5" },
  {215, 95, 100, 65, TFT_ORANGE,  "6" },
  // Reihe 3
  {  5,165, 100, 65, TFT_PURPLE,  "7" },
  {110,165, 100, 65, TFT_YELLOW,  "8" },
  {215,165, 100, 65, TFT_SILVER,  "9" }
};

static const int MENU_GRID_COUNT = sizeof(MENU_GRID) / sizeof(MENU_GRID[0]);

void drawMenu() {
  applyTheme();
  tft.fillScreen(BG_COLOR);
  
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Storage System", 160, 5, 2);
  tft.drawFastHLine(0, 20, SCREEN_W, BORDER_COLOR);
  
  for (int i = 0; i < MENU_GRID_COUNT; i++) {
    const AppButton& b = MENU_GRID[i];
    drawButton(b.x, b.y, b.w, b.h, b.color, b.label);
  }
}

// ================= ZAHLANZEIGE =================
int displayedNumber = 0;

void showNumber(int num) {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TFT_WHITE, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String(num), 160, 100, 7);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Zum Zuruck tippen", 160, 200, 2);
  displayedNumber = num;
}

// ================= ESP-NOW =================
// ⚠️ HIER MUSS DIE MAC-ADRESSE DES EMPFÄNGERS STEHEN (nicht Ihre eigene!)
uint8_t receiverMac[] = {0x84, 0xF7, 0x03, 0xF2, 0xFD, 0x6C}; // <-- BITTE ÄNDERN!

typedef struct struct_message {typedef struct struct_message {

  uint8_t digit;
} struct_message;

struct_message myData;

// Korrigierter Callback für neue ESP32-Core-API
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void sendDigit(int num) {
  myData.digit = (uint8_t)num;
  esp_err_t result = esp_now_send(receiverMac, (uint8_t *) &myData, sizeof(myData));
  if (result == ESP_OK) {
    Serial.println("Sending confirmed");
  } else {
    Serial.println("Sending error");
  }
}

void initESPNow() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  applyTheme();
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);
  drawMenu();
  initESPNow();
}

// ================= LOOP =================
void loop() {
  int tx, ty;
  if (getTouch(tx, ty)) {
    if (displayedNumber == 0) {
      for (int i = 0; i < MENU_GRID_COUNT; i++) {
        const AppButton& b = MENU_GRID[i];
        if (isButtonPressed(tx, ty, b.x, b.y, b.w, b.h)) {
          int num = i + 1;
          Serial.println(num);
          sendDigit(num);
          showNumber(num);
          drainTouch();
          break;
        }
      }
    } else {
      drawMenu();
      displayedNumber = 0;
      drainTouch();
    }
  }
}
