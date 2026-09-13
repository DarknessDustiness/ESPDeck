#include <Arduino.h>
#include <TFT_eSPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>

#define JOYSTICK_X_PIN 34
#define JOYSTICK_Y_PIN 35
#define JOYSTICK_SW_PIN 32

static BLEUUID MIDI_SERVICE_UUID("03b80e5a-ede8-4b33-a751-6ce34ec4c700");
static BLEUUID MIDI_CHAR_UUID("7772e5de-c348-4309-a62d-77145f959e00");

struct Patch {
    const char* name;
    uint8_t msb;
    uint8_t lsb;
    uint8_t pc;
};

struct Category {
    const char* name;
    const Patch* patches;
    uint8_t count;
};

const Patch PIANO_PATCHES[] = {
    {"Concert Grand", 0, 68, 0},
    {"Ballad Grand", 0, 68, 1},
    {"Mellow Grand", 0, 68, 2},
    {"Bright Grand", 0, 68, 3},
    {"Upright Piano", 0, 68, 4},
    {"70s E.Grand", 0, 68, 5},
    {"Harpsichord", 0, 68, 6},
    {"Harpsi. Octave", 0, 68, 7}
};

const Patch EPIANO_PATCHES[] = {
    {"1976 Suitcase", 0, 68, 8},
    {"Tine EP", 0, 68, 9},
    {"SA EP 1980", 0, 68, 10},
    {"Dyno EP", 0, 68, 11},
    {"Wurlitzer EP", 0, 68, 12},
    {"Clavinet", 0, 68, 13},
    {"Vibraphone", 0, 68, 14},
    {"Marimba", 0, 68, 15}
};

const Patch ORGAN_PATCHES[] = {
    {"Jazz Organ 1", 0, 68, 16},
    {"Jazz Organ 2", 0, 68, 17},
    {"Rock Organ", 0, 68, 18},
    {"Church Pipe 1", 0, 68, 19},
    {"Church Pipe 2", 0, 68, 20},
    {"Combo Organ", 0, 68, 21},
    {"Accordion", 0, 68, 22}
};

const Patch STRINGS_PATCHES[] = {
    {"Concert Strings", 0, 68, 23},
    {"Vocal Pad", 0, 68, 24},
    {"Synth Pad 1", 0, 68, 25},
    {"Synth Pad 2", 0, 68, 26},
    {"Choir", 0, 68, 27},
    {"Synth Brass", 0, 68, 28},
    {"Full Brass", 0, 68, 29}
};

const Patch GM2_PATCHES[] = {
    {"GM2 Grand", 121, 0, 0},
    {"GM2 Bright Piano", 121, 0, 1},
    {"GM2 El. Grand", 121, 0, 2},
    {"GM2 Honky-Tonk", 121, 0, 3},
    {"GM2 Rhodes", 121, 0, 4},
    {"GM2 Chorused EP", 121, 0, 5},
    {"GM2 Harpsi", 121, 0, 6},
    {"GM2 Clav", 121, 0, 7},
    {"GM2 Celesta", 121, 0, 8},
    {"GM2 Glocken", 121, 0, 9},
    {"GM2 Music Box", 121, 0, 10},
    {"GM2 Vibraphone", 121, 0, 11},
    {"GM2 Marimba", 121, 0, 12},
    {"GM2 Xylophone", 121, 0, 13},
    {"GM2 Tubular Bell", 121, 0, 14},
    {"GM2 Dulcimer", 121, 0, 15},
    {"GM2 Drawbar Organ", 121, 0, 16},
    {"GM2 Perc Organ", 121, 0, 17},
    {"GM2 Rock Organ", 121, 0, 18},
    {"GM2 Church Organ", 121, 0, 19},
    {"GM2 Reed Organ", 121, 0, 20},
    {"GM2 Accordion", 121, 0, 21},
    {"GM2 Harmonica", 121, 0, 22},
    {"GM2 Tango Accord", 121, 0, 23},
    {"GM2 Nylon Guitar", 121, 0, 24},
    {"GM2 Steel Guitar", 121, 0, 25},
    {"GM2 Jazz Guitar", 121, 0, 26},
    {"GM2 Clean Guitar", 121, 0, 27},
    {"GM2 Muted Guitar", 121, 0, 28},
    {"GM2 Overdrive Gtr", 121, 0, 29},
    {"GM2 Dist. Guitar", 121, 0, 30},
    {"GM2 Gtr Harmonics", 121, 0, 31},
    {"GM2 Acoust Bass", 121, 0, 32},
    {"GM2 Finger Bass", 121, 0, 33},
    {"GM2 Pick Bass", 121, 0, 34},
    {"GM2 Fretless Bass", 121, 0, 35},
    {"GM2 Slap Bass 1", 121, 0, 36},
    {"GM2 Slap Bass 2", 121, 0, 37},
    {"GM2 Synth Bass 1", 121, 0, 38},
    {"GM2 Synth Bass 2", 121, 0, 39},
    {"GM2 Violin", 121, 0, 40},
    {"GM2 Viola", 121, 0, 41},
    {"GM2 Cello", 121, 0, 42},
    {"GM2 Contrabass", 121, 0, 43},
    {"GM2 Tremolo Str", 121, 0, 44},
    {"GM2 Pizzicato", 121, 0, 45},
    {"GM2 Orchestral", 121, 0, 46},
    {"GM2 Timpani", 121, 0, 47},
    {"GM2 Strings 1", 121, 0, 48},
    {"GM2 Strings 2", 121, 0, 49},
    {"GM2 SynthStr 1", 121, 0, 50},
    {"GM2 SynthStr 2", 121, 0, 51},
    {"GM2 Choir Aahs", 121, 0, 52},
    {"GM2 Voice Oohs", 121, 0, 53},
    {"GM2 Synth Voice", 121, 0, 54},
    {"GM2 Orchestral Hit", 121, 0, 55},
    {"GM2 Trumpet", 121, 0, 56},
    {"GM2 Trombone", 121, 0, 57},
    {"GM2 Tuba", 121, 0, 58},
    {"GM2 Muted Trumpet", 121, 0, 59},
    {"GM2 French Horn", 121, 0, 60},
    {"GM2 Brass Section", 121, 0, 61},
    {"GM2 Synth Brass 1", 121, 0, 62},
    {"GM2 Synth Brass 2", 121, 0, 63},
    {"GM2 Soprano Sax", 121, 0, 64},
    {"GM2 Alto Sax", 121, 0, 65},
    {"GM2 Tenor Sax", 121, 0, 66},
    {"GM2 Baritone Sax", 121, 0, 67},
    {"GM2 Oboe", 121, 0, 68},
    {"GM2 English Horn", 121, 0, 69},
    {"GM2 Bassoon", 121, 0, 70},
    {"GM2 Clarinet", 121, 0, 71},
    {"GM2 Piccolo", 121, 0, 72},
    {"GM2 Flute", 121, 0, 73},
    {"GM2 Recorder", 121, 0, 74},
    {"GM2 Pan Flute", 121, 0, 75},
    {"GM2 Blown Bottle", 121, 0, 76},
    {"GM2 Shakuhachi", 121, 0, 77},
    {"GM2 Whistle", 121, 0, 78},
    {"GM2 Ocarina", 121, 0, 79},
    {"GM2 Square Lead", 121, 0, 80},
    {"GM2 Saw Lead", 121, 0, 81},
    {"GM2 Calliope", 121, 0, 82},
    {"GM2 Chiff Lead", 121, 0, 83},
    {"GM2 Charang", 121, 0, 84},
    {"GM2 Voice Lead", 121, 0, 85},
    {"GM2 Fifths Lead", 121, 0, 86},
    {"GM2 Bass & Lead", 121, 0, 87},
    {"GM2 New Age Pad", 121, 0, 88},
    {"GM2 Warm Pad", 121, 0, 89},
    {"GM2 PolySynth Pad", 121, 0, 90},
    {"GM2 Choir Pad", 121, 0, 91},
    {"GM2 Bowed Pad", 121, 0, 92},
    {"GM2 Metallic Pad", 121, 0, 93},
    {"GM2 Halo Pad", 121, 0, 94},
    {"GM2 Sweep Pad", 121, 0, 95}
};

const Category CATEGORIES[] = {
    {"Piano", PIANO_PATCHES, sizeof(PIANO_PATCHES) / sizeof(Patch)},
    {"E.Piano", EPIANO_PATCHES, sizeof(EPIANO_PATCHES) / sizeof(Patch)},
    {"Organ", ORGAN_PATCHES, sizeof(ORGAN_PATCHES) / sizeof(Patch)},
    {"Strings", STRINGS_PATCHES, sizeof(STRINGS_PATCHES) / sizeof(Patch)},
    {"GM2 Bank", GM2_PATCHES, sizeof(GM2_PATCHES) / sizeof(Patch)}
};

const int NUM_CATEGORIES = sizeof(CATEGORIES) / sizeof(Category);

class RolandBLEController {
private:
    BLEClient* pClient = nullptr;
    BLERemoteCharacteristic* pRemoteChar = nullptr;
    bool connected = false;
    bool scanning = false;
    BLEAdvertisedDevice* targetDevice = nullptr;

    class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
        RolandBLEController& parent;
    public:
        AdvertisedDeviceCallbacks(RolandBLEController& p) : parent(p) {}
        void onResult(BLEAdvertisedDevice advertisedDevice) override {
            if (advertisedDevice.isAdvertisingService(MIDI_SERVICE_UUID)) {
                BLEDevice::getScan()->stop();
                parent.targetDevice = new BLEAdvertisedDevice(advertisedDevice);
                parent.scanning = false;
            }
        }
    };

public:
    void init() {
        BLEDevice::init("ESP32_MIDI_Ctrl");
    }

    void startScan() {
        if (connected || scanning) return;
        scanning = true;
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(*this));
        pBLEScan->setInterval(1349);
        pBLEScan->setWindow(449);
        pBLEScan->setActiveScan(true);
        // Non-blocking 1-second scan to prevent Watchdog Timer timeouts
        pBLEScan->start(1, false);
    }

    void update() {
        if (!connected && targetDevice != nullptr) {
            connect();
        } else if (!connected && !scanning) {
            static unsigned long lastScanTime = 0;
            if (millis() - lastScanTime > 3000) {
                lastScanTime = millis();
                startScan();
            }
        }
    }

    bool connect() {
        pClient = BLEDevice::createClient();
        if (!pClient->connect(targetDevice)) {
            delete targetDevice;
            targetDevice = nullptr;
            return false;
        }
        BLERemoteService* pRemoteService = pClient->getService(MIDI_SERVICE_UUID);
        if (pRemoteService == nullptr) {
            pClient->disconnect();
            delete targetDevice;
            targetDevice = nullptr;
            return false;
        }
        pRemoteChar = pRemoteService->getCharacteristic(MIDI_CHAR_UUID);
        if (pRemoteChar == nullptr) {
            pClient->disconnect();
            delete targetDevice;
            targetDevice = nullptr;
            return false;
        }
        connected = true;
        delete targetDevice;
        targetDevice = nullptr;
        return true;
    }

    bool isConnected() const { return connected; }

    void sendVolume(uint8_t vol) {
        if (!connected || !pRemoteChar) return;
        uint8_t packet[5] = {0x80, 0x80, 0xB0, 0x07, (uint8_t)(vol & 0x7F)};
        pRemoteChar->writeValue(packet, 5, false);
    }

    void sendFullPatch(uint8_t msb, uint8_t lsb, uint8_t pc) {
        if (!connected || !pRemoteChar) return;
        uint8_t ccMsb[5] = {0x80, 0x80, 0xB0, 0x00, (uint8_t)(msb & 0x7F)};
        pRemoteChar->writeValue(ccMsb, 5, false);
        uint8_t ccLsb[5] = {0x80, 0x80, 0xB0, 0x20, (uint8_t)(lsb & 0x7F)};
        pRemoteChar->writeValue(ccLsb, 5, false);
        uint8_t prog[4]  = {0x80, 0x80, 0xC0, (uint8_t)(pc & 0x7F)};
        pRemoteChar->writeValue(prog, 4, false);
    }
};

class WaveScreensaver {
private:
    uint8_t sinTable[256];
    uint16_t fadeLUT[64];
    uint8_t phase = 0;

public:
    void init(TFT_eSprite &canvas) {
        for (int i = 0; i < 256; i++) {
            sinTable[i] = (uint8_t)((sin(i * 2.0 * PI / 256.0) + 1.0) * 127.5);
        }
        for (int i = 0; i < 64; i++) {
            uint8_t r = 160 + ((255 - 160) * i) / 63;
            uint8_t g = 32  + ((255 - 32)  * i) / 63;
            uint8_t b = 240 + ((255 - 240) * i) / 63;
            fadeLUT[i] = canvas.color565(r, g, b);
        }
    }

    void draw(TFT_eSprite &canvas, int minY = 50, int maxY = 110) {
        uint16_t solidPurple = fadeLUT[0];

        for (int x = 0; x < 160; x++) {
            uint8_t idx1 = (x * 1 + phase) % 256;
            uint8_t idx2 = (x * 2 - phase) % 256;

            int smoothSine = ((int)sinTable[idx1] * 3 + (int)sinTable[idx2]) / 4;
            int y1 = minY + (smoothSine * 12 / 128); 

            uint8_t idx3 = (x * 1 - phase) % 256;
            uint8_t idx4 = (x * 2 + phase * 2) % 256;
            int fadeSine = ((int)sinTable[idx3] * 3 + (int)sinTable[idx4]) / 4;
            int y2 = y1 + 10 + (fadeSine * 8 / 128); 

            if (y1 >= maxY) continue;
            if (y2 < y1) y2 = y1;
            if (y2 > maxY) y2 = maxY;

            int solidHeight = y2 - y1;
            if (solidHeight > 0) {
                canvas.drawFastVLine(x, y1, solidHeight, solidPurple);
            }

            int fadeDist = maxY - y2;
            if (fadeDist > 0) {
                for (int y = y2; y < maxY; y++) {
                    int lutIndex = ((y - y2) * 63) / fadeDist;
                    canvas.drawPixel(x, y, fadeLUT[lutIndex]);
                }
            }
        }
        phase += 3;
    }
};

enum class Views {
    Waves,
    Laptop,
    Roland,
    MSR,
    Transit
};

enum class MenuState {
    Volume,
    CategorySelect,
    PatchSelect
};

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
WaveScreensaver waveSaver;
RolandBLEController rolandBLE;

const Views VIEW_ORDER[] = {Views::Waves, Views::Laptop, Views::Roland, Views::MSR};
const int NUM_VIEWS = 4;

int currentViewIndex = 0;
int previousViewIndex = 0;
Views c_view = Views::Waves;
Views previousView = Views::Waves;
Views targetView = Views::Waves;

int transitX = 0;
int transitDir = 0;          
bool joystickCentered = true; 
bool joystickYCentered = true;

int rolandVolume = 100;
int currentCategoryIdx = 0;
int currentPatchIdx = 0;
MenuState currentMenuState = MenuState::Volume;
unsigned long lastBtnPress = 0;

void setup() {
    Serial.begin(115200);
    pinMode(JOYSTICK_SW_PIN, INPUT_PULLUP);

    // 1. Force screen clear first to verify SPI display hardware
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Booting...", 10, 10, 2);

    // 2. Allocate Sprite buffer before BLE stack reservation
    if (spr.createSprite(160, 128) == nullptr) {
        Serial.println("Error: Insufficient RAM for TFT Sprite!");
        tft.drawString("RAM Error!", 10, 30, 2);
        while (1) yield();
    }

    tft.drawString("Init Screen OK", 10, 30, 2);

    // 3. Initialize screensaver and BLE controller
    waveSaver.init(spr);
    
    tft.drawString("Init BLE...", 10, 50, 2);
    rolandBLE.init();
    
    tft.drawString("Ready!", 10, 70, 2);
    delay(300);
}

int getJoystickXDirection() {
    int rawX = analogRead(JOYSTICK_X_PIN);
    if (rawX > 3000) return -1;
    if (rawX < 1000) return 1;
    return 0;
}

int getJoystickYDirection() {
    int rawY = analogRead(JOYSTICK_Y_PIN);
    if (rawY > 3000) return 1;
    if (rawY < 1000) return -1;
    return 0;
}

void handleRolandInputs() {
    rolandBLE.update();

    if (digitalRead(JOYSTICK_SW_PIN) == LOW && (millis() - lastBtnPress > 250)) {
        lastBtnPress = millis();
        if (currentMenuState == MenuState::Volume) {
            currentMenuState = MenuState::CategorySelect;
        } else if (currentMenuState == MenuState::CategorySelect) {
            currentMenuState = MenuState::PatchSelect;
            currentPatchIdx = 0;
        } else {
            currentMenuState = MenuState::Volume;
            const Patch& p = CATEGORIES[currentCategoryIdx].patches[currentPatchIdx];
            rolandBLE.sendFullPatch(p.msb, p.lsb, p.pc);
        }
    }

    int dirY = getJoystickYDirection();
    if (dirY == 0) {
        joystickYCentered = true;
    } else if (joystickYCentered) {
        joystickYCentered = false;
        
        if (currentMenuState == MenuState::Volume) {
            rolandVolume = constrain(rolandVolume + (dirY * 5), 0, 127);
            rolandBLE.sendVolume(rolandVolume);
        } else if (currentMenuState == MenuState::CategorySelect) {
            currentCategoryIdx += dirY;
            if (currentCategoryIdx >= NUM_CATEGORIES) currentCategoryIdx = 0;
            if (currentCategoryIdx < 0) currentCategoryIdx = NUM_CATEGORIES - 1;
        } else if (currentMenuState == MenuState::PatchSelect) {
            int maxPatches = CATEGORIES[currentCategoryIdx].count;
            currentPatchIdx += dirY;
            if (currentPatchIdx >= maxPatches) currentPatchIdx = 0;
            if (currentPatchIdx < 0) currentPatchIdx = maxPatches - 1;
            
            const Patch& p = CATEGORIES[currentCategoryIdx].patches[currentPatchIdx];
            rolandBLE.sendFullPatch(p.msb, p.lsb, p.pc);
        }
    }
}

void renderRolandView(TFT_eSprite &canvas) {
    canvas.fillSprite(TFT_BLACK);
    
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawString("FP-30X CTRL", 8, 8, 2);

    if (rolandBLE.isConnected()) {
        canvas.fillRoundRect(110, 8, 42, 14, 3, canvas.color565(0, 180, 80));
        canvas.setTextColor(TFT_WHITE, canvas.color565(0, 180, 80));
        canvas.drawString("BLE OK", 114, 11, 1);
    } else {
        canvas.fillRoundRect(100, 8, 52, 14, 3, canvas.color565(200, 40, 40));
        canvas.setTextColor(TFT_WHITE, canvas.color565(200, 40, 40));
        canvas.drawString("SEARCH...", 103, 11, 1);
    }

    canvas.drawFastHLine(0, 26, 160, canvas.color565(40, 40, 60));

    const Patch& activePatch = CATEGORIES[currentCategoryIdx].patches[currentPatchIdx];

    canvas.setTextColor(canvas.color565(150, 150, 170), TFT_BLACK);
    canvas.drawString("Category:", 8, 32, 1);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.drawString(CATEGORIES[currentCategoryIdx].name, 65, 32, 1);

    canvas.setTextColor(canvas.color565(150, 150, 170), TFT_BLACK);
    canvas.drawString("Patch:", 8, 46, 1);
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.drawString(activePatch.name, 65, 46, 1);

    canvas.setTextColor(canvas.color565(150, 150, 170), TFT_BLACK);
    canvas.drawString("Volume:", 8, 66, 1);
    
    int volWidth = map(rolandVolume, 0, 127, 0, 144);
    canvas.drawRect(8, 78, 144, 10, canvas.color565(60, 60, 80));
    canvas.fillRect(9, 79, volWidth, 8, canvas.color565(0, 150, 255));

    if (currentMenuState == MenuState::CategorySelect) {
        canvas.fillRoundRect(6, 28, 148, 64, 4, canvas.color565(15, 20, 35));
        canvas.drawRoundRect(6, 28, 148, 64, 4, TFT_CYAN);
        canvas.setTextColor(TFT_CYAN, canvas.color565(15, 20, 35));
        canvas.drawString("SELECT CATEGORY", 28, 34, 1);

        for (int i = -1; i <= 1; i++) {
            int idx = currentCategoryIdx + i;
            if (idx < 0) idx = NUM_CATEGORIES - 1;
            if (idx >= NUM_CATEGORIES) idx = 0;

            if (i == 0) {
                canvas.setTextColor(TFT_YELLOW, canvas.color565(15, 20, 35));
                canvas.drawString("> " + String(CATEGORIES[idx].name) + " <", 20, 56, 2);
            }
        }
    } else if (currentMenuState == MenuState::PatchSelect) {
        canvas.fillRoundRect(6, 28, 148, 64, 4, canvas.color565(25, 15, 35));
        canvas.drawRoundRect(6, 28, 148, 64, 4, TFT_YELLOW);
        canvas.setTextColor(TFT_YELLOW, canvas.color565(25, 15, 35));
        canvas.drawString("SELECT SOUND", 36, 34, 1);

        canvas.setTextColor(TFT_WHITE, canvas.color565(25, 15, 35));
        canvas.drawString(CATEGORIES[currentCategoryIdx].patches[currentPatchIdx].name, 12, 56, 2);
    }
}

void renderView(Views v, TFT_eSprite &canvas) {
    switch (v) {
        case Views::Waves:
            waveSaver.draw(canvas, 50, 110);
            break;

        case Views::Laptop:
            canvas.setTextColor(TFT_WHITE, TFT_BLACK);
            canvas.drawString("Laptop View", 40, 50, 1);
            break;

        case Views::Roland:
            renderRolandView(canvas);
            break;

        case Views::MSR:
            canvas.setTextColor(TFT_GREEN, TFT_BLACK);
            canvas.drawString("MSR View", 50, 50, 1);
            break;

        default:
            break;
    }
}

void drawBottomBar(TFT_eSprite &canvas, int activeIndex, int prevIndex, bool isTransiting, int transitProgress) {
    int barY = 121;
    int spacing = 18;
    int totalWidth = (NUM_VIEWS - 1) * spacing;
    int startX = (160 - totalWidth) / 2;

    int fromX = startX + prevIndex * spacing;
    int toX = startX + activeIndex * spacing;

    int capsuleX;
    if (isTransiting) {
        float progress = (float)transitProgress / 160.0f;

        if (prevIndex == NUM_VIEWS - 1 && activeIndex == 0) {
            capsuleX = fromX + (int)(progress * spacing);
            if (capsuleX > startX + (NUM_VIEWS - 1) * spacing + (spacing / 2)) {
                capsuleX -= (NUM_VIEWS * spacing);
            }
        } else if (prevIndex == 0 && activeIndex == NUM_VIEWS - 1) {
            capsuleX = fromX - (int)(progress * spacing);
            if (capsuleX < startX - (spacing / 2)) {
                capsuleX += (NUM_VIEWS * spacing);
            }
        } else {
            capsuleX = fromX + (int)(progress * (toX - fromX));
        }
    } else {
        capsuleX = toX;
    }

    canvas.fillRect(0, 114, 160, 14, canvas.color565(12, 12, 18));
    canvas.drawFastHLine(0, 114, 160, canvas.color565(35, 35, 45));

    for (int i = 0; i < NUM_VIEWS; i++) {
        int dotX = startX + i * spacing;
        canvas.fillCircle(dotX, barY, 1, canvas.color565(70, 70, 90));
    }

    uint16_t glowColor = canvas.color565(0, 150, 255);
    canvas.fillRoundRect(capsuleX - 7, barY - 2, 15, 5, 2, glowColor);
    canvas.fillRoundRect(capsuleX - 5, barY - 1, 11, 3, 1, TFT_WHITE);
}

void loop() {
    spr.fillSprite(TFT_BLACK);

    if (c_view == Views::Roland || targetView == Views::Roland) {
        handleRolandInputs();
    } else {
        rolandBLE.update();
    }

    int dir = getJoystickXDirection();

    if (c_view != Views::Transit) {
        if (dir == 0) {
            joystickCentered = true; 
        } 
        else if (joystickCentered && dir != 0) {
            joystickCentered = false; 

            int nextIndex = currentViewIndex + dir;

            if (nextIndex >= NUM_VIEWS) {
                nextIndex = 0;
            } else if (nextIndex < 0) {
                nextIndex = NUM_VIEWS - 1;
            }

            previousView = c_view;
            previousViewIndex = currentViewIndex;
            currentViewIndex = nextIndex;
            targetView = VIEW_ORDER[currentViewIndex];

            transitDir = dir;
            transitX = 0;
            c_view = Views::Transit;
        }
    }

    if (c_view == Views::Transit) {
        transitX += 12;

        int outX = (transitDir == 1) ? -transitX : transitX;
        int inX   = (transitDir == 1) ? (160 - transitX) : (-160 + transitX);

        spr.setViewport(outX, 0, 160, 115);
        renderView(previousView, spr);

        spr.setViewport(inX, 0, 160, 115);
        renderView(targetView, spr);

        spr.resetViewport();

        if (transitX >= 160) {
            c_view = targetView;
            transitX = 0;
        }
    } else {
        spr.setViewport(0, 0, 160, 115);
        renderView(c_view, spr);
        spr.resetViewport();
    }

    bool isTransiting = (c_view == Views::Transit);
    drawBottomBar(spr, currentViewIndex, previousViewIndex, isTransiting, transitX);

    spr.pushSprite(0, 0);
    delay(16);
}