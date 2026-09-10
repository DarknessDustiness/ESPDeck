#include <Arduino.h>
#include <TFT_eSPI.h>

#define JOYSTICK_X_PIN 34 

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

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
WaveScreensaver waveSaver;

const Views VIEW_ORDER[] = {Views::Waves, Views::Laptop, Views::Roland, Views::MSR};
const int NUM_VIEWS = 4;

// Global State Variables
int currentViewIndex = 0;
int previousViewIndex = 0; // Fixed: Declared missing variable
Views c_view = Views::Waves;
Views previousView = Views::Waves;
Views targetView = Views::Waves;

int transitX = 0;
int transitDir = 0;          
bool joystickCentered = true; 

void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(3);
    spr.createSprite(160, 128);

    waveSaver.init(spr);
}

int getJoystickDirection() {
    int rawX = analogRead(JOYSTICK_X_PIN);
    
    if (rawX > 3000) return -1;   // Reversed direction
    if (rawX < 1000) return 1;    // Reversed direction
    return 0;                     // Deadzone
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
            canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
            canvas.drawString("Roland View", 45, 50, 1);
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

    int dir = getJoystickDirection();

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
            previousViewIndex = currentViewIndex; // Fixed: Track previous index here
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
        int inX  = (transitDir == 1) ? (160 - transitX) : (-160 + transitX);

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