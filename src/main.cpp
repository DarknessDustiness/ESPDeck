#include <Arduino.h>
#include <TFT_eSPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>

// --- Hardware Pin Definitions ---
#define JOYSTICK_X_PIN  34
#define JOYSTICK_Y_PIN  35
#define JOYSTICK_SW_PIN 32
#define BUTTON_BACK_PIN 33

// --- MIDI BLE UUIDs for Roland FP-30X ---
static BLEUUID midiServiceUUID("03B80E5A-EDE8-4B33-A751-6CE34E4C7000");
static BLEUUID midiCharUUID("7772E528-C0F2-4F2B-9E61-317329598C71");

// --- Screen & Rendering Setup ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// --- Roland FP-30X Preset Data Structure ---
struct RolandTone {
  const char* name;
  uint8_t msb;
  uint8_t lsb;
  uint8_t pc;
};

const RolandTone TONE_BANK[] = {
  {"Concert Grand", 0, 0, 0},
  {"Ballad Grand",  0, 0, 1},
  {"Mellow Grand",  0, 0, 2},
  {"Bright Grand",  0, 0, 3},
  {"1976 E.Piano",  0, 0, 4},
  {"Tine E.Piano",  0, 0, 5},
  {"Synth Bell",    0, 0, 10},
  {"Pipe Organ",    0, 0, 14},
  {"Combo Organ",   0, 0, 15},
  {"Church Organ",  0, 0, 16},
  {"Rotary Organ",  0, 0, 17},
  {"Strings 1",     0, 0, 19},
  {"GM2 Jazz Organ",121,0, 16},
  {"GM2 Synth Brass",121,0, 62}
};
const int TOTAL_TONES = sizeof(TONE_BANK) / sizeof(RolandTone);

// --- State Variables ---
enum AppScreen { SCREEN_CONTROL, SCREEN_SAVER };
AppScreen currentScreen = SCREEN_CONTROL;

int selectedToneIndex = 0;
int activeToneIndex = 0;
uint8_t lastRotaryVal = 255;
unsigned long lastInputTime = 0;
const unsigned long IDLE_TIMEOUT_MS = 30000; // 30s auto screensaver

// --- BLE Client State ---
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
BLEClient* pClient = nullptr;
bool bleConnected = false;
bool bleConnecting = false;
BLEAdvertisedDevice* targetDevice = nullptr;

// --- Wave Screensaver Class ---
class WaveScreensaver {
private:
  uint8_t sinTable[256];
  uint16_t fadeLUT[64];
  uint8_t phase = 0;

public:
  void init(TFT_eSprite &spr) {
    for (int i = 0; i < 256; i++) {
      sinTable[i] = (uint8_t)((sin(i * 2.0 * PI / 256.0) + 1.0) * 127.5);
    }
    for (int i = 0; i < 64; i++) {
      uint8_t r = 30  + (225 * i) / 63;
      uint8_t g = 140 + (115 * i) / 63;
      uint8_t b = 255 - (100 * i) / 63;
      fadeLUT[i] = spr.color565(r, g, b);
    }
  }

  void render(TFT_eSprite &spr) {
    spr.fillSprite(TFT_BLACK);
    
    // Header overlay
    spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
    spr.drawString("ROLAND FP-30X IDLE", 25, 10, 2);

    for (int x = 0; x < 160; x++) {
      uint8_t idx1 = (x * 1 + phase) % 256;
      uint8_t idx2 = (x * 2 - phase) % 256;
      int smoothSine = ((int)sinTable[idx1] * 3 + (int)sinTable[idx2]) / 4;
      int y1 = 45 + (smoothSine * 20 / 128);

      uint8_t idx3 = (x * 1 - phase) % 256;
      uint8_t idx4 = (x * 2 + phase * 2) % 256;
      int fadeSine = ((int)sinTable[idx3] * 3 + (int)sinTable[idx4]) / 4;
      int y2 = y1 + 12 + (fadeSine * 10 / 128);

      if (y1 >= 120) continue;
      if (y2 < y1) y2 = y1;
      if (y2 > 120) y2 = 120;

      int solidHeight = y2 - y1;
      if (solidHeight > 0) {
        spr.drawFastVLine(x, y1, solidHeight, fadeLUT[0]);
      }

      int fadeDist = 120 - y2;
      if (fadeDist > 0) {
        for (int y = y2; y < 120; y++) {
          int lutIndex = ((y - y2) * 63) / fadeDist;
          spr.drawPixel(x, y, fadeLUT[lutIndex]);
        }
      }
    }
    phase += 4;
    spr.pushSprite(0, 0);
  }
};

WaveScreensaver screensaver;

// --- MIDI BLE Packet Transmit Helper ---
void sendBLEMIDICommand(uint8_t status, uint8_t data1, uint8_t data2 = 0xFF) {
  if (!bleConnected || pRemoteCharacteristic == nullptr) return;

  uint8_t packet[5];
  packet[0] = 0x80; // Header byte with timestamp
  packet[1] = 0x80; // Timestamp byte
  packet[2] = status;
  packet[3] = data1;

  size_t len = 4;
  if (data2 != 0xFF) {
    packet[4] = data2;
    len = 5;
  }

  pRemoteCharacteristic->writeValue(packet, len, false);
}

void selectRolandTone(int index) {
  if (index < 0 || index >= TOTAL_TONES) return;
  RolandTone tone = TONE_BANK[index];

  // Send CC#0 (Bank Select MSB), CC#32 (Bank Select LSB), Program Change
  sendBLEMIDICommand(0xB0, 0x00, tone.msb);
  sendBLEMIDICommand(0xB0, 0x20, tone.lsb);
  sendBLEMIDICommand(0xC0, tone.pc);

  activeToneIndex = index;
}

void sendRotarySpeed(uint8_t val) {
  // CC#1 Modulation Wheel drives Leslie speed on Roland engines
  sendBLEMIDICommand(0xB0, 0x01, val);
}

// --- BLE Scanner Callbacks ---
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.isAdvertisingService(midiServiceUUID)) {
      BLEDevice::getScan()->stop();
      targetDevice = new BLEAdvertisedDevice(advertisedDevice);
      bleConnecting = true;
    }
  }
};

bool connectToPiano() {
  pClient = BLEDevice::createClient();
  if (!pClient->connect(targetDevice)) return false;

  BLERemoteService* pRemoteService = pClient->getService(midiServiceUUID);
  if (pRemoteService == nullptr) {
    pClient->disconnect();
    return false;
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(midiCharUUID);
  if (pRemoteCharacteristic == nullptr) {
    pClient->disconnect();
    return false;
  }

  bleConnected = true;
  bleConnecting = false;
  return true;
}

// --- UI Drawing Engine ---
void renderControlUI() {
  canvas.fillSprite(TFT_BLACK);

  // Status Header
  canvas.fillRect(0, 0, 160, 18, canvas.color565(20, 20, 30));
  if (bleConnected) {
    canvas.setTextColor(TFT_GREEN, canvas.color565(20, 20, 30));
    canvas.drawString("BLE: CONNECTED", 4, 2, 2);
  } else if (bleConnecting) {
    canvas.setTextColor(TFT_YELLOW, canvas.color565(20, 20, 30));
    canvas.drawString("BLE: CONNECTING...", 4, 2, 2);
  } else {
    canvas.setTextColor(TFT_RED, canvas.color565(20, 20, 30));
    canvas.drawString("BLE: SCANNING...", 4, 2, 2);
  }

  // Active Sound Title Box
  canvas.drawRect(4, 22, 152, 26, canvas.color565(0, 180, 220));
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.drawString("ACTIVE TONE:", 8, 25, 1);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString(TONE_BANK[activeToneIndex].name, 8, 35, 2);

  // Scrollable Tone Menu Window (Displays 3 items)
  int startIdx = max(0, selectedToneIndex - 1);
  if (startIdx + 3 > TOTAL_TONES) startIdx = max(0, TOTAL_TONES - 3);

  for (int i = 0; i < 3; i++) {
    int itemIdx = startIdx + i;
    if (itemIdx >= TOTAL_TONES) break;

    int y = 52 + (i * 18);
    if (itemIdx == selectedToneIndex) {
      canvas.fillRect(4, y, 152, 16, canvas.color565(0, 120, 200));
      canvas.setTextColor(TFT_WHITE, canvas.color565(0, 120, 200));
    } else {
      canvas.setTextColor(TFT_SILVER, TFT_BLACK);
    }
    canvas.drawString(TONE_BANK[itemIdx].name, 8, y + 1, 2);
  }

  // Real-Time Rotary Bar
  canvas.setTextColor(TFT_GOLD, TFT_BLACK);
  canvas.drawString("ROTARY SPEED:", 4, 108, 1);
  canvas.drawRect(80, 108, 76, 10, TFT_WHITE);
  int fillWidth = map((lastRotaryVal == 255) ? 0 : lastRotaryVal, 0, 127, 0, 72);
  canvas.fillRect(82, 110, fillWidth, 6, TFT_GOLD);

  canvas.pushSprite(0, 0);
}

// --- Setup & Loop ---
void setup() {
  Serial.begin(115200);

  // Configure Inputs
  pinMode(JOYSTICK_SW_PIN, INPUT_PULLUP);
  pinMode(BUTTON_BACK_PIN, INPUT_PULLUP);
  analogSetAttenuation(ADC_11db);

  // Initialize Display & Sprite
  tft.init();
  tft.setRotation(1); // 160x128 Landscape
  canvas.createSprite(160, 128);
  screensaver.init(canvas);

  // BLE Initialization
  BLEDevice::init("ESP32_FP30X_Ctrl");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->start(0, false);

  lastInputTime = millis();
}

void loop() {
  unsigned long now = millis();

  // BLE Connection Handler
  if (bleConnecting && targetDevice != nullptr) {
    if (!connectToPiano()) {
      bleConnecting = false;
      BLEDevice::getScan()->start(0, false);
    }
  }

  // Process Inputs
  int rawX = analogRead(JOYSTICK_X_PIN);
  int rawY = analogRead(JOYSTICK_Y_PIN);
  bool selectPressed = (digitalRead(JOYSTICK_SW_PIN) == LOW);
  bool backPressed = (digitalRead(BUTTON_BACK_PIN) == LOW);

  static unsigned long lastNavTime = 0;
  bool activityDetected = false;

  // Screensaver Wakeup Check
  if (currentScreen == SCREEN_SAVER) {
    if (rawY < 1000 || rawY > 3000 || rawX < 1000 || rawX > 3000 || selectPressed || backPressed) {
      currentScreen = SCREEN_CONTROL;
      lastInputTime = now;
      delay(200); // Debounce
      return;
    }
    screensaver.render(canvas);
    delay(16); // ~60 FPS update
    return;
  }

  // Navigation Y-Axis (Tone Menu)
  if (now - lastNavTime > 180) {
    if (rawY < 1000) { // Up
      selectedToneIndex = max(0, selectedToneIndex - 1);
      lastNavTime = now;
      activityDetected = true;
    } else if (rawY > 3000) { // Down
      selectedToneIndex = min(TOTAL_TONES - 1, selectedToneIndex + 1);
      lastNavTime = now;
      activityDetected = true;
    }
  }

  // Continuous Control X-Axis (Live Rotary Speaker Modulation)
  if (rawX < 1200 || rawX > 2800) {
    uint8_t rotaryVal = map(constrain(rawX, 0, 4095), 0, 4095, 0, 127);
    if (abs(rotaryVal - lastRotaryVal) > 2) {
      lastRotaryVal = rotaryVal;
      sendRotarySpeed(rotaryVal);
      activityDetected = true;
    }
  }

  // Select Switch (Apply Tone)
  static bool lastSelectState = HIGH;
  bool currentSelectState = digitalRead(JOYSTICK_SW_PIN);
  if (lastSelectState == HIGH && currentSelectState == LOW) {
    selectRolandTone(selectedToneIndex);
    activityDetected = true;
  }
  lastSelectState = currentSelectState;

  // Back Button (Manual Screensaver trigger)
  if (backPressed) {
    currentScreen = SCREEN_SAVER;
    delay(200);
    return;
  }

  // Idle Timeout check
  if (activityDetected) lastInputTime = now;
  if (now - lastInputTime > IDLE_TIMEOUT_MS) {
    currentScreen = SCREEN_SAVER;
  }

  renderControlUI();
  delay(20);
}