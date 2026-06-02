#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =========================
// GPIO Buttons
// =========================
#define BTN_MIDDLE 14
#define BTN_RIGHT 13
#define BTN_DOWN 12

// =========================
// Analog thresholds
// =========================
int analogLowThreshold = 500;
int analogHighThreshold = 700;

// =========================
// Menu
// =========================
String menuItems[] = {
  "SLEEP",
  "WIFI",
  "GAME"
};

int menuIndex = 0;

// =========================
// Game Variables
// =========================
int playerY = 48;
int playerVelocity = 0;
bool jumping = false;

int cactusX = 120;
int score = 0;
bool dead = false;

// =========================
// WiFi
// =========================
struct WifiEntry {
  String ssid;
  int rssi;
};

WifiEntry wifiList[5];
int wifiCount = 0;

// =========================
// Button enum
// =========================
enum Button {
  BTN_NONE,
  BTN_UP,
  BTN_LEFT,
  BTN_RIGHT_PRESS,
  BTN_DOWN_PRESS,
  BTN_MIDDLE_PRESS
};

// =========================
// Read averaged ADC
// =========================
int readADC() {
  long sum = 0;

  for (int i = 0; i < 10; i++) {
    sum += analogRead(A0);
    delay(2);
  }

  return sum / 10;
}

// ========================= 
// Detect buttons 
// ========================= 
Button getButton() { 
  int val = readADC();
  // -------- UP -------- 
  // Lower analog voltage 
  if (val < analogLowThreshold) { 
    return BTN_UP; 
  }
  // -------- LEFT -------- 
  // Between UP and idle 
  if (val < analogHighThreshold && val > analogLowThreshold + 30) { 
    return BTN_LEFT; 
  }
  // -------- RIGHT -------- 
  if (!digitalRead(BTN_RIGHT)) { 
    return BTN_RIGHT_PRESS; 
  }
  // -------- DOWN -------- 
  if (!digitalRead(BTN_DOWN)) { 
    return BTN_DOWN_PRESS; 
  }
  // -------- MIDDLE -------- 
  if (!digitalRead(BTN_MIDDLE)) { 
    return BTN_MIDDLE_PRESS; 
  }
  return BTN_NONE; 
}
// =========================
// Wait for release
// =========================
void waitRelease() {
  while (getButton() != BTN_NONE) {
    delay(10);
  }
}

// =========================
// Draw centered text
// =========================
void drawCentered(String text, int y) {
  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);

  display.setCursor((128 - w) / 2, y);
  display.print(text);
}

// ========================= 
// Calibration 
// ========================= 
void calibrateButtons() {

  // ===================== 
  // CALIBRATE UP 
  // ===================== 
  display.clearDisplay(); 
  drawCentered("PRESS UP", 20); 
  drawCentered("TO CALIBRATE", 35);
  display.display();
  while (true) { 
    int val = readADC(); 
    // Wait for a clear voltage drop 
    if (val < 700) { 
      analogLowThreshold = val + 20;
      break; 
    } 
    delay(10); 
  } 
  waitRelease();

  // ===================== 
  // CALIBRATE LEFT 
  // ===================== 
  display.clearDisplay(); 
  drawCentered("PRESS LEFT", 20); 
  drawCentered("TO CALIBRATE", 35); 
  display.display();
  while (true) { 
    int val = readADC(); 
    // Must be higher than UP 
    // but lower than idle 
    if (val < 850 && val > analogLowThreshold + 30) { 
      analogHighThreshold = val + 20; 
      break; 
    } 
    delay(10); 
  } 
  waitRelease();
  // ===================== 
  // DONE SCREEN 
  // ===================== 
  display.clearDisplay(); 
  display.setCursor(0, 10); 
  display.print("UP THRESHOLD:"); 
  display.setCursor(0, 20); 
  display.println(analogLowThreshold); 
  display.setCursor(0, 40); 
  display.print("LEFT THRESHOLD:"); 
  display.setCursor(0, 50); 
  display.println(analogHighThreshold); 
  display.display(); 
  delay(2000); 
}

// =========================
// Menu Draw
// =========================
void drawMenu() {

  display.clearDisplay();

  for (int i = 0; i < 3; i++) {

    int x = 10 + (i * 40);

    if (i == menuIndex) {
      display.fillRect(x - 2, 20, 36, 18, WHITE);
      display.setTextColor(BLACK);
    }
    else {
      display.setTextColor(WHITE);
    }

    display.setCursor(x, 25);
    display.print(menuItems[i]);
  }

  display.setTextColor(WHITE);

  display.display();
}

// =========================
// Deep Sleep
// =========================
void goSleep() {

  display.clearDisplay();
  drawCentered("SLEEPING", 28);
  display.display();

  delay(1000);

  ESP.deepSleep(0);
}

// =========================
// WIFI SCAN
// =========================
void runWifiScanner() {

  while (true) {

    display.clearDisplay();
    drawCentered("SCANNING...", 0);
    display.display();

    int n = WiFi.scanNetworks();

    wifiCount = min(n, 5);

    for (int i = 0; i < wifiCount; i++) {
      wifiList[i].ssid = WiFi.SSID(i);
      wifiList[i].rssi = WiFi.RSSI(i);
    }

    display.clearDisplay();

    for (int i = 0; i < wifiCount; i++) {

      display.setCursor(0, i * 12);
      display.print(wifiList[i].ssid);

      display.setCursor(90, i * 12);
      display.print(wifiList[i].rssi);
      display.print("dBm");
    }

    display.setCursor(0, 56);
    display.print("HOLD MID = BACK");

    display.display();

    unsigned long holdStart = millis();

    while (millis() - holdStart < 1500) {

      if (getButton() == BTN_MIDDLE_PRESS) {
        delay(20);
      }
      else {
        break;
      }
    }

    if (millis() - holdStart >= 1500) {
      waitRelease();
      return;
    }

    delay(10000);
  }
}

// =========================
// Reset Game
// =========================
void resetGame() {

  playerY = 48;
  playerVelocity = 0;
  jumping = false;

  cactusX = 120;
  score = 0;
  dead = false;
}

// =========================
// Run Game
// =========================
void runGame() {

  resetGame();

  while (true) {

    Button btn = getButton();

    // Exit with hold middle
    if (btn == BTN_MIDDLE_PRESS) {

      unsigned long hold = millis();

      while (getButton() == BTN_MIDDLE_PRESS) {

        if (millis() - hold > 1200) {
          waitRelease();
          return;
        }
      }
    }

    if (dead) {

      display.clearDisplay();

      drawCentered("GAME OVER", 18);

      display.setCursor(35, 38);
      display.print("Score: ");
      display.print(score);

      display.setCursor(10, 54);
      display.print("MID = RESET");

      display.display();

      if (btn == BTN_MIDDLE_PRESS) {
        resetGame();
        waitRelease();
      }

      delay(20);
      continue;
    }

    // Jump
    if (btn == BTN_UP && !jumping) {
      playerVelocity = -8;
      jumping = true;
    }

    // Crouch
    bool crouch = false;

    if (btn == BTN_DOWN_PRESS) {
      crouch = true;
    }

    // Gravity
    playerVelocity += 1;
    playerY += playerVelocity;

    if (playerY >= 48) {
      playerY = 48;
      playerVelocity = 0;
      jumping = false;
    }

    // Move cactus
    cactusX -= 4;

    if (cactusX < -10) {
      cactusX = 128;
      score++;
    }

    // Collision
    int playerHeight = crouch ? 8 : 16;
    int playerWidth = 10;

    if (cactusX < 20 && cactusX > 8) {

      if (playerY + playerHeight > 40) {
        dead = true;
      }
    }

    // Draw
    display.clearDisplay();

    // Ground
    display.drawLine(0, 56, 128, 56, WHITE);

    // Player
    display.fillRect(10, playerY, playerWidth, playerHeight, WHITE);

    // Cactus
    display.fillRect(cactusX, 40, 8, 16, WHITE);

    // Score
    display.setCursor(90, 0);
    display.print(score);

    display.display();

    delay(30);
  }
}

// =========================
// Setup
// =========================
void setup() {

  pinMode(BTN_MIDDLE, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  Serial.begin(115200);

  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  calibrateButtons();
}

// =========================
// Main Loop
// =========================
void loop() {

  drawMenu();

  Button btn = getButton();

  // Scroll right
  if (btn == BTN_RIGHT_PRESS) {

    menuIndex++;

    if (menuIndex > 2) {
      menuIndex = 0;
    }

    waitRelease();
  }

  // Scroll left
  if (btn == BTN_LEFT) {

    menuIndex--;

    if (menuIndex < 0) {
      menuIndex = 2;
    }

    waitRelease();
  }

  // Select with middle
  if (btn == BTN_MIDDLE_PRESS) {

    waitRelease();

    switch (menuIndex) {

      case 0:
        goSleep();
        break;

      case 1:
        runWifiScanner();
        break;

      case 2:
        runGame();
        break;
    }
  }

  delay(50);
}

