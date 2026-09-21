#include "face.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include <SPI.h>
#include "pico_link.h"

// OLED wiring - VERIFIED, do not change without updating docs/CURRENT_STATE.md.
static const int OLED_SCLK = 47;
static const int OLED_MOSI = 21;
static const int OLED_RST  = 2;
static const int OLED_DC   = 41;
static const int OLED_CS   = 42;

static Adafruit_SSD1331 display = Adafruit_SSD1331(
    OLED_CS, OLED_DC, OLED_MOSI, OLED_SCLK, OLED_RST);

static FaceState currentState = FACE_IDLE;

// --- blink / look-around: shared by the two states with pupils (IDLE, ANGRY) ---
static unsigned long lastBlinkTime = 0;
static unsigned long blinkStartTime = 0;
static unsigned long nextBlinkDelay = 3000;
static bool eyesClosed = false;
static const unsigned long BLINK_DURATION = 150;

static unsigned long lastLookTime = 0;
static unsigned long lookStartTime = 0;
static unsigned long nextLookDelay = 2500;
static unsigned long lookDuration = 800;
static bool lookingAside = false;
static int pupilOffset = 0;

// --- thinking pulse ---
static unsigned long lastThinkingTime = 0;
static bool thinkingBright = true;
static const unsigned long THINKING_INTERVAL = 350;

// --- speaking waveform ---
static unsigned long lastSpeakingTime = 0;
static const unsigned long SPEAKING_INTERVAL = 90;

// --- sleeping breathing pulse ---
static unsigned long lastSleepTime = 0;
static bool sleepBright = false;
static const unsigned long SLEEP_INTERVAL = 900;

// --- glitch flicker ---
static unsigned long lastGlitchTime = 0;
static const unsigned long GLITCH_INTERVAL = 80;

static void drawAngryEyes(int offset) {
    display.fillScreen(0x0000);
    display.fillTriangle(20, 22, 40, 27, 38, 34, 0xF800);
    display.fillTriangle(20, 22, 38, 34, 20, 32, 0xF800);
    display.fillTriangle(56, 27, 76, 22, 76, 32, 0xF800);
    display.fillTriangle(56, 27, 76, 32, 58, 34, 0xF800);
    display.fillRect(29 + offset, 26, 3, 5, 0x0000);
    display.fillRect(65 + offset, 26, 3, 5, 0x0000);
}

static void drawIdleEyes(int offset) {
    display.fillScreen(0x0000);
    display.fillRoundRect(20 + offset, 24, 20, 10, 4, 0xFFE0);
    display.fillRoundRect(56 + offset, 24, 20, 10, 4, 0xFFE0);
    display.fillRect(27 + offset, 27, 3, 4, 0x0000);
    display.fillRect(63 + offset, 27, 3, 4, 0x0000);
}

static void drawOpenEyesForState(FaceState state, int offset) {
    if (state == FACE_ANGRY) {
        drawAngryEyes(offset);
    } else {
        drawIdleEyes(offset);
    }
}

static void drawClosedEyes(uint16_t color) {
    display.fillScreen(0x0000);
    display.fillRect(20, 28, 20, 2, color);
    display.fillRect(56, 28, 20, 2, color);
}

static void drawListeningEyes() {
    display.fillScreen(0x0000);
    display.fillCircle(30, 28, 9, 0x07FF);
    display.fillCircle(66, 28, 9, 0x07FF);
}

static void drawThinkingEyes(uint16_t color) {
    display.fillScreen(0x0000);
    display.fillRoundRect(19, 24, 22, 9, 4, color);
    display.fillRoundRect(55, 24, 22, 9, 4, color);
}

static void drawSurprisedEyes() {
    display.fillScreen(0x0000);
    display.fillCircle(29, 28, 12, 0xFFFF);
    display.fillCircle(67, 28, 12, 0xFFFF);
    display.fillCircle(29, 28, 4, 0x0000);
    display.fillCircle(67, 28, 4, 0x0000);
}

static void drawSuspiciousEyes() {
    display.fillScreen(0x0000);
    display.fillRoundRect(18, 27, 24, 5, 2, 0xF800);
    display.fillRoundRect(54, 27, 24, 5, 2, 0xF800);
}

static void drawSleepingEyes(bool bright) {
    display.fillScreen(0x0000);
    uint16_t color = bright ? 0x39E7 : 0x18E3;  // dim bluish-grey "breathing" pulse
    display.fillRect(20, 29, 20, 2, color);
    display.fillRect(56, 29, 20, 2, color);
    display.setTextColor(color);
    display.setCursor(78, 6);
    display.print("z");
}

static void drawSpeakingFrame() {
    display.fillScreen(0x0000);
    display.fillRoundRect(22, 22, 16, 8, 3, 0x07FF);
    display.fillRoundRect(58, 22, 16, 8, 3, 0x07FF);
    for (int i = 0; i < 6; i++) {
        int barHeight = random(3, 16);
        display.fillRect(18 + i * 10, 48 - barHeight, 6, barHeight, 0x07FF);
    }
}

static void drawGlitchFrame() {
    display.fillScreen(0x0000);
    for (int i = 0; i < 5; i++) {
        int x = random(0, 90);
        int y = random(0, 58);
        int w = random(4, 20);
        int h = random(2, 10);
        uint16_t color = random(0, 2) ? 0xF800 : 0x07FF;
        display.fillRect(x, y, w, h, color);
    }
}

static void resetTimersForState(unsigned long now) {
    eyesClosed = false;
    lastBlinkTime = now;
    nextBlinkDelay = random(2000, 6000);

    lookingAside = false;
    pupilOffset = 0;
    lastLookTime = now;
    nextLookDelay = random(1500, 4000);
}

static void drawStateFrame(FaceState state) {
    switch (state) {
        case FACE_IDLE:
        case FACE_ANGRY:
            drawOpenEyesForState(state, 0);
            break;
        case FACE_LISTENING:
            drawListeningEyes();
            break;
        case FACE_THINKING:
            drawThinkingEyes(0x801F);
            break;
        case FACE_SURPRISED:
            drawSurprisedEyes();
            break;
        case FACE_SUSPICIOUS:
            drawSuspiciousEyes();
            break;
        case FACE_SLEEPING:
            drawSleepingEyes(false);
            break;
        case FACE_SPEAKING:
            drawSpeakingFrame();
            break;
        case FACE_GLITCH:
            drawGlitchFrame();
            break;
    }
}

void faceInit() {
    display.begin();
    currentState = FACE_IDLE;
    resetTimersForState(millis());
    drawStateFrame(currentState);
}

FaceState getFace() {
    return currentState;
}

void setFace(FaceState newState) {
    currentState = newState;
    resetTimersForState(millis());
    drawStateFrame(currentState);
    picoLedFace(faceStateName(currentState));  // shield LEDs mirror the OLED "emotion"
}

const char *faceStateName(FaceState state) {
    switch (state) {
        case FACE_IDLE: return "IDLE";
        case FACE_ANGRY: return "ANGRY";
        case FACE_LISTENING: return "LISTENING";
        case FACE_THINKING: return "THINKING";
        case FACE_SPEAKING: return "SPEAKING";
        case FACE_SURPRISED: return "SURPRISED";
        case FACE_SUSPICIOUS: return "SUSPICIOUS";
        case FACE_SLEEPING: return "SLEEPING";
        case FACE_GLITCH: return "GLITCH";
    }
    return "IDLE";
}

static void updateBlinkAndLook(unsigned long now) {
    if (!eyesClosed && now - lastBlinkTime >= nextBlinkDelay) {
        drawClosedEyes(currentState == FACE_ANGRY ? 0xF800 : 0xFFE0);
        eyesClosed = true;
        blinkStartTime = now;
    }

    if (eyesClosed && now - blinkStartTime >= BLINK_DURATION) {
        drawOpenEyesForState(currentState, pupilOffset);
        eyesClosed = false;
        lastBlinkTime = now;
        nextBlinkDelay = random(2000, 6000);
    }

    if (eyesClosed) return;

    if (!lookingAside && now - lastLookTime >= nextLookDelay) {
        pupilOffset = (random(0, 2) == 0) ? -3 : 3;
        drawOpenEyesForState(currentState, pupilOffset);
        lookingAside = true;
        lookStartTime = now;
        lookDuration = random(700, 1300);
    }

    if (lookingAside && now - lookStartTime >= lookDuration) {
        pupilOffset = 0;
        drawOpenEyesForState(currentState, pupilOffset);
        lookingAside = false;
        lastLookTime = now;
        nextLookDelay = random(1500, 4000);
    }
}

void faceUpdate(unsigned long now) {
    switch (currentState) {
        case FACE_IDLE:
        case FACE_ANGRY:
            updateBlinkAndLook(now);
            break;

        case FACE_THINKING:
            if (now - lastThinkingTime >= THINKING_INTERVAL) {
                thinkingBright = !thinkingBright;
                drawThinkingEyes(thinkingBright ? 0x801F : 0x4010);
                lastThinkingTime = now;
            }
            break;

        case FACE_SPEAKING:
            if (now - lastSpeakingTime >= SPEAKING_INTERVAL) {
                drawSpeakingFrame();
                lastSpeakingTime = now;
            }
            break;

        case FACE_SLEEPING:
            if (now - lastSleepTime >= SLEEP_INTERVAL) {
                sleepBright = !sleepBright;
                drawSleepingEyes(sleepBright);
                lastSleepTime = now;
            }
            break;

        case FACE_GLITCH:
            if (now - lastGlitchTime >= GLITCH_INTERVAL) {
                drawGlitchFrame();
                lastGlitchTime = now;
            }
            break;

        case FACE_LISTENING:
        case FACE_SURPRISED:
        case FACE_SUSPICIOUS:
            break;  // static frame, nothing to animate
    }
}
