#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include <SPI.h>

const int OLED_SCLK = 47;
const int OLED_MOSI = 21;
const int OLED_RST  = 2;
const int OLED_DC   = 41;
const int OLED_CS   = 42;

enum FaceState {
    ANGRY,
    LISTENING,
    THINKING
};

FaceState currentState = ANGRY;

unsigned long lastBlinkTime = 0;
unsigned long blinkStartTime = 0;
unsigned long nextBlinkDelay = 3000;

bool eyesClosed = false;

const unsigned long BLINK_DURATION = 150;

unsigned long lastLookTime = 0;
unsigned long lookStartTime = 0;
unsigned long nextLookDelay = 2500;
unsigned long lookDuration = 800;

bool lookingAside = false;
int pupilOffset = 0;

unsigned long lastThinkingTime = 0;
bool thinkingBright = true;

const unsigned long THINKING_INTERVAL = 350;

Adafruit_SSD1331 display = Adafruit_SSD1331(
    OLED_CS,
    OLED_DC,
    OLED_MOSI,
    OLED_SCLK,
    OLED_RST
);

void drawAngryEyes(int pupilOffset) {
    display.fillScreen(0x0000);

    // Ojo izquierdo
    display.fillTriangle(
        20, 22,
        40, 27,
        38, 34,
        0xF800
    );

    display.fillTriangle(
        20, 22,
        38, 34,
        20, 32,
        0xF800
    );

    // Ojo derecho
    display.fillTriangle(
        56, 27,
        76, 22,
        76, 32,
        0xF800
    );

    display.fillTriangle(
        56, 27,
        76, 32,
        58, 34,
        0xF800
    );
    display.fillRect(29 + pupilOffset, 26, 3, 5, 0x0000);
    display.fillRect(65 + pupilOffset, 26, 3, 5, 0x0000);
}

void drawClosedEyes() {
    display.fillScreen(0x0000);

    display.fillRect(20, 28, 20, 2, 0xF800);
    display.fillRect(56, 28, 20, 2, 0xF800);
}

void drawListeningEyes() {
    display.fillScreen(0x0000);

    display.fillCircle(30, 28, 9, 0x07FF);
    display.fillCircle(66, 28, 9, 0x07FF);
}

void drawThinkingEyes(uint16_t color) {
    display.fillScreen(0x0000);

    display.fillRoundRect(19, 24, 22, 9, 4, color);
    display.fillRoundRect(55, 24, 22, 9, 4, color);
}

void updateThinking(unsigned long now) {
    if (now - lastThinkingTime >= THINKING_INTERVAL) {
        thinkingBright = !thinkingBright;

        if (thinkingBright) {
            drawThinkingEyes(0x801F);
        } else {
            drawThinkingEyes(0x4010);
        }

        lastThinkingTime = now;
    }
}

void drawFace() {
    switch (currentState) {
        case ANGRY:
            drawAngryEyes(0);
            break;

        case LISTENING:
            drawListeningEyes();
            break;
        case THINKING:
            drawThinkingEyes(0x801F);
            break;    
    }

}

void setFaceState(FaceState newState) {
    currentState = newState;

    eyesClosed = false;
    lastBlinkTime = millis();
    nextBlinkDelay = random(2000, 6000);

    lookingAside = false;
    pupilOffset = 0;
    lastLookTime = millis();
    nextLookDelay = random(1500, 4000);

    drawFace();
}

void updateBlink(unsigned long now) {
    if (!eyesClosed && now - lastBlinkTime >= nextBlinkDelay) {
        drawClosedEyes();

        eyesClosed = true;
        blinkStartTime = now;
    }

    if (eyesClosed && now - blinkStartTime >= BLINK_DURATION) {
        drawAngryEyes(pupilOffset);

        eyesClosed = false;
        lastBlinkTime = now;
        nextBlinkDelay = random(2000, 6000);
    }
}

void updateLookAround(unsigned long now) {
    if (!lookingAside && now - lastLookTime >= nextLookDelay) {
        if (random(0, 2) == 0) {
            pupilOffset = -3;
        } else {
            pupilOffset = 3;
        }

        drawAngryEyes(pupilOffset);

        lookingAside = true;
        lookStartTime = now;
        lookDuration = random(700, 1300);
    }

    if (lookingAside && now - lookStartTime >= lookDuration) {
        pupilOffset = 0;
        drawAngryEyes(pupilOffset);

        lookingAside = false;
        lastLookTime = now;
        nextLookDelay = random(1500, 4000);
    }
}

void setup() {Serial.begin(115200);
  Serial.println("Mini-Ultron online");

  display.begin();
  drawFace();
}

void loop() {
    unsigned long now = millis();

    if (Serial.available() > 0) {
        char command = Serial.read();

        if (command == 'a') {
            setFaceState(ANGRY);
        }

        if (command == 'l') {
            setFaceState(LISTENING);
        }

        if (command == 't') {
            setFaceState(THINKING);
        }
    }

    if (currentState == ANGRY) {
        updateBlink(now);

        if (!eyesClosed) {
            updateLookAround(now);
        }
    }

    if (currentState == THINKING) {
        updateThinking(now);
    }
}
