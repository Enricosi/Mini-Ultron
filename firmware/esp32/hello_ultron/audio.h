/**********************************************************************
  Audio - INMP441 mic in + Freenove amp out, ONE full-duplex I2S port
  (GPIO9/10 shared clocks, mic SD=GPIO11, amp DIN=GPIO12, no MCLK - see
  docs/CURRENT_STATE.md). Wiring/format proven by mic_speaker_test.

  Runs in its own FreeRTOS task on core 0 (ESP_I2S has no non-blocking
  write, and a blocked loop() would freeze the face). The main loop only
  touches this module through the functions below; nothing here blocks.

  Continuous listening: an energy-based voice detector (adaptive noise
  floor, hysteresis, pre-roll, hangover) finds utterances. The mic is
  ignored while the robot is playing audio (plus a short tail) so it
  never hears itself. Detected utterances are kept in RAM (last one only,
  up to AUDIO_CAPTURE_SECONDS) for verification via AUDIO:REPLAY; streaming
  to Django comes later.
**********************************************************************/
#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>

enum AudioEvent {
  AUDIO_EVENT_NONE,
  AUDIO_EVENT_SPEECH_START,  // an utterance began
  AUDIO_EVENT_SPEECH_END,    // a valid utterance finished (captured)
  AUDIO_EVENT_SPEECH_ABORT   // began but was too short / interrupted, discarded
};

struct AudioStatus {
  int level;             // last 20 ms RMS (mic units, DC removed)
  int noiseFloor;        // adaptive silence estimate
  int startThreshold;
  int stopThreshold;
  bool calibrated;       // first ~1 s of silence measured
  bool speechActive;
  bool playing;
  uint32_t utterances;   // valid utterances since boot
  uint32_t lastUtteranceMs;
  uint32_t capturedSamples;  // samples of the last valid capture (16 kHz)
};

void audioInit();                 // starts the audio task
AudioEvent audioPollEvent();      // main loop: returns next event, clears it
AudioStatus audioGetStatus();
bool audioIsPlaying();
bool audioBeep();                 // short test beep; false if busy
bool audioReplayLastUtterance();  // false if none captured or busy

// --- CEREBRO integration ---
// Last valid utterance: 16 kHz mono int16, up to 12 s (PSRAM). Valid until the next
// utterance starts being captured; audioDeafAfterUtterance() protects it meanwhile.
const int16_t *audioCapturedPcm(size_t *samples);
// Play a reply (16 kHz mono int16, e.g. CEREBRO's TTS). The buffer must stay valid
// until audioIsPlaying() is false. False if busy.
bool audioPlayPcm16(const int16_t *pcm, size_t samples);
// Ignore the mic (robot is thinking/speaking). No utterance is detected while deaf.
void audioSetDeaf(bool deaf);
// If enabled, the audio task itself goes deaf the moment an utterance ends, so a new
// one can never overwrite the capture buffer while it is being sent.
void audioDeafAfterUtterance(bool enable);

#endif
