#include "pico_link.h"
#include "pico_link_config.h"

#if defined(PICO_LINK_USE_HARDWARE_UART) && (PICO_LINK_RX_PIN < 0 || PICO_LINK_TX_PIN < 0)
#error "PICO_LINK_USE_HARDWARE_UART is defined but RX/TX pins are still TBD - set real GPIO numbers in pico_link_config.h first."
#endif

static Stream *picoPort = nullptr;

static char rxBuffer[64];
static uint8_t rxLen = 0;

static PicoResponseType lastType = PICO_RESP_NONE;
static float lastDistanceCm = -1.0f;
static char lastLineBits[4] = "???";
static bool everHeardFromPico = false;

void picoLinkInit() {
#ifdef PICO_LINK_USE_HARDWARE_UART
    Serial1.begin(PICO_LINK_BAUD, SERIAL_8N1, PICO_LINK_RX_PIN, PICO_LINK_TX_PIN);
    picoPort = &Serial1;
#else
    picoPort = nullptr;
#endif
    rxLen = 0;
}

bool picoLinkIsConnected() {
    return picoPort != nullptr;
}

static void sendLine(const String &line) {
    if (picoPort != nullptr) {
        picoPort->print(line);
        picoPort->print('\n');
    } else {
        Serial.print("[picoLink] no UART configured, would send: ");
        Serial.println(line);
    }
}

static void handleResponseLine(const String &line) {
    everHeardFromPico = true;

    if (line == "READY") {
        lastType = PICO_RESP_READY;
    } else if (line == "OK") {
        lastType = PICO_RESP_OK;
    } else if (line == "OBSTACLE") {
        lastType = PICO_RESP_OBSTACLE;
    } else if (line == "STOPPED:TIMEOUT") {
        lastType = PICO_RESP_STOPPED_TIMEOUT;
    } else if (line.startsWith("ERR:")) {
        lastType = PICO_RESP_ERR;
    } else if (line.startsWith("DIST:")) {
        lastType = PICO_RESP_DIST;
        lastDistanceCm = line.substring(5).toFloat();
    } else if (line.startsWith("LINE:")) {
        lastType = PICO_RESP_LINE;
        line.substring(5, 8).toCharArray(lastLineBits, sizeof(lastLineBits));
    } else {
        lastType = PICO_RESP_UNKNOWN;
    }
}

void picoLinkPoll() {
    if (picoPort == nullptr) return;

    while (picoPort->available() > 0) {
        char c = picoPort->read();
        if (c == '\n') {
            rxBuffer[rxLen] = '\0';
            handleResponseLine(String(rxBuffer));
            rxLen = 0;
        } else if (c != '\r') {
            if (rxLen < sizeof(rxBuffer) - 1) {
                rxBuffer[rxLen++] = c;
            }
        }
    }
}

PicoResponseType picoLastResponseType() {
    return lastType;
}

float picoLastDistanceCm() {
    return lastDistanceCm;
}

const char *picoLastLineBits() {
    return lastLineBits;
}

bool picoIsAlive() {
    return everHeardFromPico;
}

// --- high-level command API ---

static int clampSpeed(int speedPercent) {
    return constrain(speedPercent, 0, 100);
}

void picoPing() {
    sendLine("PING");
}

void picoStop() {
    sendLine("STOP");
}

void picoStatus() {
    sendLine("STATUS");
}

void picoMoveForward(int speedPercent) {
    sendLine("MOVE:FWD:" + String(clampSpeed(speedPercent)));
}

void picoMoveBackward(int speedPercent) {
    sendLine("MOVE:BACK:" + String(clampSpeed(speedPercent)));
}

void picoMoveLeft(int speedPercent) {
    sendLine("MOVE:LEFT:" + String(clampSpeed(speedPercent)));
}

void picoMoveRight(int speedPercent) {
    sendLine("MOVE:RIGHT:" + String(clampSpeed(speedPercent)));
}

void picoRotateLeft(int speedPercent) {
    sendLine("ROTATE:L:" + String(clampSpeed(speedPercent)));
}

void picoRotateRight(int speedPercent) {
    sendLine("ROTATE:R:" + String(clampSpeed(speedPercent)));
}

void picoNeckCenter() {
    sendLine("NECK:CENTER");
}

void picoNeckLeft() {
    sendLine("NECK:LEFT");
}

void picoNeckRight() {
    sendLine("NECK:RIGHT");
}

void picoLedOn() {
    sendLine("LED:ON");
}

void picoLedOff() {
    sendLine("LED:OFF");
}
