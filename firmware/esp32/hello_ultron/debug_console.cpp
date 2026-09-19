#include "debug_console.h"
#include <Arduino.h>
#include "face.h"
#include "pico_link.h"
#include "arm_gestures.h"
#include "audio.h"
#include "imu.h"
#include "brain_link.h"
#include "motion.h"

static char buffer[64];
static uint8_t len = 0;

static void printHelp() {
    Serial.println(F(
        "Mini-Ultron debug console - full commands, newline-terminated:\n"
        "  FACE:IDLE|ANGRY|LISTENING|THINKING|SPEAKING|SURPRISED|SUSPICIOUS|SLEEPING|GLITCH\n"
        "  PICO:PING | PICO:STOP | PICO:STATUS | PICO:LAST\n"
        "  NECK:CENTER | NECK:LEFT | NECK:RIGHT\n"
        "  MOVE:FWD:<0-100> | MOVE:BACK:<0-100> | MOVE:LEFT:<0-100> | MOVE:RIGHT:<0-100>\n"
        "  ROTATE:L:<0-100> | ROTATE:R:<0-100>\n"
        "  AUDIO:STATUS | AUDIO:BEEP | AUDIO:REPLAY (replays the last detected utterance)\n"
        "  BRAIN:STATUS (wifi / CEREBRO link) | MOTION:STOP\n"
        "  IMU:STATUS | IMU:CAL (keep the robot still for ~1 s)\n"
        "  ARMS:NEUTRAL|UP|LEFT|RIGHT|WAVE (deferred in v1, see arm_gestures.h)\n"
        "  HELP"));
}

static void handleFace(const String &name) {
    if (name == "IDLE") setFace(FACE_IDLE);
    else if (name == "ANGRY") setFace(FACE_ANGRY);
    else if (name == "LISTENING") setFace(FACE_LISTENING);
    else if (name == "THINKING") setFace(FACE_THINKING);
    else if (name == "SPEAKING") setFace(FACE_SPEAKING);
    else if (name == "SURPRISED") setFace(FACE_SURPRISED);
    else if (name == "SUSPICIOUS") setFace(FACE_SUSPICIOUS);
    else if (name == "SLEEPING") setFace(FACE_SLEEPING);
    else if (name == "GLITCH") setFace(FACE_GLITCH);
    else {
        Serial.println("ERR:PARSE");
        return;
    }
    Serial.println("OK");
}

static const char *picoResponseTypeName(PicoResponseType type) {
    switch (type) {
        case PICO_RESP_NONE: return "NONE";
        case PICO_RESP_READY: return "READY";
        case PICO_RESP_OK: return "OK";
        case PICO_RESP_ERR: return "ERR";
        case PICO_RESP_OBSTACLE: return "OBSTACLE";
        case PICO_RESP_STOPPED_TIMEOUT: return "STOPPED_TIMEOUT";
        case PICO_RESP_DIST: return "DIST";
        case PICO_RESP_LINE: return "LINE";
        default: return "UNKNOWN";
    }
}

static void handlePico(const String &action) {
    if (action == "PING") picoPing();
    else if (action == "STOP") picoStop();
    else if (action == "STATUS") picoStatus();
    else if (action == "LAST") {
        Serial.print("PICO:ALIVE=");
        Serial.print(picoIsAlive() ? "1" : "0");
        Serial.print(" TYPE=");
        Serial.print(picoResponseTypeName(picoLastResponseType()));
        Serial.print(" DIST=");
        Serial.print(picoLastDistanceCm());
        Serial.print(" LINE=");
        Serial.println(picoLastLineBits());
        return;
    }
    else {
        Serial.println("ERR:PARSE");
        return;
    }
    Serial.println("OK");
}

static void handleNeck(const String &dir) {
    if (dir == "CENTER") picoNeckCenter();
    else if (dir == "LEFT") picoNeckLeft();
    else if (dir == "RIGHT") picoNeckRight();
    else {
        Serial.println("ERR:PARSE");
        return;
    }
    Serial.println("OK");
}

static void handleAudio(const String &action) {
    if (action == "STATUS") {
        AudioStatus a = audioGetStatus();
        Serial.printf("AUDIO: LEVEL=%d FLOOR=%d START_THR=%d STOP_THR=%d CALIBRATED=%d "
                      "SPEECH=%d PLAYING=%d UTTERANCES=%u LAST_MS=%u CAPTURED_SAMPLES=%u\n",
                      a.level, a.noiseFloor, a.startThreshold, a.stopThreshold,
                      a.calibrated, a.speechActive, a.playing,
                      (unsigned)a.utterances, (unsigned)a.lastUtteranceMs,
                      (unsigned)a.capturedSamples);
        return;
    }
    bool ok;
    if (action == "BEEP") ok = audioBeep();
    else if (action == "REPLAY") ok = audioReplayLastUtterance();
    else {
        Serial.println("ERR:PARSE");
        return;
    }
    Serial.println(ok ? "OK" : "ERR:BUSY_OR_NOTHING_CAPTURED");
}

static void handleBrain(const String &action) {
    if (action == "STATUS") brainPrintStatus();
    else Serial.println("ERR:PARSE");
}

static void handleMotion(const String &action) {
    if (action == "STOP") {
        motionAbort();
        picoStop();
        Serial.println("OK");
    } else {
        Serial.println("ERR:PARSE");
    }
}

static void handleImu(const String &action) {
    if (action == "STATUS") {
        ImuData d = imuGetData();
        Serial.printf("IMU: OK=%d CALIBRATED=%d CALIBRATING=%d ACC[g]=%.2f,%.2f,%.2f "
                      "GYRO[dps]=%.1f,%.1f,%.1f ROLL=%.1f PITCH=%.1f JOLTS=%u LAST_JOLT_AGO_MS=%ld READS_OK=%u READS_FAILED=%u\n",
                      d.ok, d.calibrated, d.calibrating, d.ax, d.ay, d.az,
                      d.gx, d.gy, d.gz, d.rollDeg, d.pitchDeg,
                      (unsigned)d.joltCount,
                      d.lastJoltMs ? (long)(millis() - d.lastJoltMs) : -1L,
                      (unsigned)d.readsOk, (unsigned)d.readsFailed);
    } else if (action == "CAL") {
        imuStartCalibration();
        Serial.println("OK");
    } else {
        Serial.println("ERR:PARSE");
    }
}

static void handleArms(const String &action) {
#if MINI_ULTRON_ENABLE_ARMS
    if (action == "NEUTRAL") handsNeutral();
    else if (action == "UP") handsUp();
    else if (action == "LEFT") leftHandOut();
    else if (action == "RIGHT") rightHandOut();
    else if (action == "WAVE") waveRightHand();
    else {
        Serial.println("ERR:PARSE");
        return;
    }
    Serial.println("OK");
#else
    (void)action;
    Serial.println("ERR:ARMS_DISABLED");
#endif
}

static void handleMoveOrRotate(const String &verb, const String &rest) {
    int secondColon = rest.indexOf(':');
    if (secondColon < 0) {
        Serial.println("ERR:PARSE");
        return;
    }
    String dir = rest.substring(0, secondColon);
    int speed = rest.substring(secondColon + 1).toInt();

    if (verb == "MOVE") {
        if (dir == "FWD") picoMoveForward(speed);
        else if (dir == "BACK") picoMoveBackward(speed);
        else if (dir == "LEFT") picoMoveLeft(speed);
        else if (dir == "RIGHT") picoMoveRight(speed);
        else {
            Serial.println("ERR:PARSE");
            return;
        }
    } else {  // ROTATE
        if (dir == "L") picoRotateLeft(speed);
        else if (dir == "R") picoRotateRight(speed);
        else {
            Serial.println("ERR:PARSE");
            return;
        }
    }
    Serial.println("OK");
}

static void handleLine(const String &line) {
    if (line.length() == 0) return;
    if (line == "HELP") {
        printHelp();
        return;
    }

    int firstColon = line.indexOf(':');
    if (firstColon < 0) {
        Serial.println("ERR:PARSE");
        return;
    }
    String verb = line.substring(0, firstColon);
    String rest = line.substring(firstColon + 1);

    if (verb == "FACE") { handleFace(rest); return; }
    if (verb == "PICO") { handlePico(rest); return; }
    if (verb == "NECK") { handleNeck(rest); return; }
    if (verb == "AUDIO") { handleAudio(rest); return; }
    if (verb == "BRAIN") { handleBrain(rest); return; }
    if (verb == "MOTION") { handleMotion(rest); return; }
    if (verb == "IMU") { handleImu(rest); return; }
    if (verb == "ARMS") { handleArms(rest); return; }
    if (verb == "MOVE" || verb == "ROTATE") { handleMoveOrRotate(verb, rest); return; }

    Serial.println("ERR:PARSE");
}

void debugConsoleInit() {
    len = 0;
}

void debugConsolePoll() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n') {
            buffer[len] = '\0';
            handleLine(String(buffer));
            len = 0;
        } else if (c != '\r') {
            if (len < sizeof(buffer) - 1) {
                buffer[len++] = c;
            }
        }
    }
}
