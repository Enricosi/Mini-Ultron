/**********************************************************************
  Mini-Ultron - ESP32-S3 interaction controller.

  Owns the OLED face (face.h), the deterministic link to the Pico W
  locomotion/neck/safety controller (pico_link.h), mic + speaker audio
  with continuous voice detection (audio.h), and a development-
  only debug console (debug_console.h). Arm/PCA9685 gestures are
  preserved but deferred from v1 (arm_gestures.h).

  Type HELP over USB Serial (115200 baud) for the debug command list.
**********************************************************************/
#include "face.h"
#include "pico_link.h"
#include "audio.h"
#include "arm_gestures.h"
#include "debug_console.h"

// Temporary face reactions to audio until Django drives the face: LISTENING while
// an utterance is being heard, SPEAKING while playing; restores the previous face.
static FaceState faceBeforeAudio = FACE_IDLE;
static bool wasPlaying = false;

static void audioFaceGlue() {
    AudioEvent e;
    while ((e = audioPollEvent()) != AUDIO_EVENT_NONE) {
        if (e == AUDIO_EVENT_SPEECH_START) {
            faceBeforeAudio = getFace();
            setFace(FACE_LISTENING);
            Serial.println("[audio] speech start");
        } else if (e == AUDIO_EVENT_SPEECH_END) {
            setFace(faceBeforeAudio);
            AudioStatus a = audioGetStatus();
            Serial.printf("[audio] speech end: %u ms, %u samples captured\n",
                          (unsigned)a.lastUtteranceMs, (unsigned)a.capturedSamples);
        } else if (e == AUDIO_EVENT_SPEECH_ABORT) {
            setFace(faceBeforeAudio);
            Serial.println("[audio] speech ignored (too short/interrupted)");
        }
    }

    bool playing = audioIsPlaying();
    if (playing && !wasPlaying) {
        faceBeforeAudio = getFace();
        setFace(FACE_SPEAKING);
    } else if (!playing && wasPlaying) {
        setFace(faceBeforeAudio);
    }
    wasPlaying = playing;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Mini-Ultron online");

    faceInit();
    picoLinkInit();
    audioInit();
    debugConsoleInit();

#if MINI_ULTRON_ENABLE_ARMS
    armsInit();
#endif

    Serial.println("Type HELP for debug commands.");
}

void loop() {
    unsigned long now = millis();

    debugConsolePoll();
    picoLinkPoll();
    audioFaceGlue();
    faceUpdate(now);
}
