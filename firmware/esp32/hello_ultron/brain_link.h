/**********************************************************************
  Link to CEREBRO (the Django brain on the home-lab server) over Wi-Fi/HTTP.

  One request in flight at a time, in a FreeRTOS task so the face and UART
  keep running while the server thinks. Two kinds of request share the same
  BUSY/DONE result shape:
    POST /api/converse  (utterance PCM16 mono 16 kHz)  -> JSON behavior + audio URL
    POST /api/event     ({"event":"JOLT",...})          -> same JSON, no utterance
    GET  <audio url>                                    -> reply PCM16 mono 16 kHz
  The reply is kept in PSRAM; conversation.cpp hands it to the audio module.

  Everything the server sends is treated as untrusted intent: only exact
  vocabulary names are accepted downstream (see conversation.cpp / motion.h).
  If wifi_config.h is missing (brain_config.h) this module is disabled and inert.
**********************************************************************/
#ifndef BRAIN_LINK_H
#define BRAIN_LINK_H

#include <Arduino.h>

enum BrainState {
  BRAIN_DISABLED,  // no wifi_config.h
  BRAIN_IDLE,
  BRAIN_BUSY,      // a request is in flight
  BRAIN_DONE       // a result is waiting for brainTakeResult()
};

struct BrainResult {
  bool ok;          // false = network/server error, see `error`
  bool ignored;     // server heard no speech / too short
  bool hasAudio;
  char face[16];    // exact CEREBRO names, validated by the caller
  char movement[16];
  char neck[8];
  char animation[25];  // "NONE" or a built-in animation name (anim_library.cpp), any case
  char error[48];
  size_t replySamples;   // valid when hasAudio (16 kHz mono int16 in brainReplyPcm())
  uint32_t httpMs;       // POST round trip (includes server STT+LLM+TTS)
  uint32_t serverSttMs, serverLlmMs, serverTtsMs;
  uint32_t downloadMs;   // reply audio GET
};

void brainInit();
BrainState brainState();
bool brainWifiConnected();
bool brainSubmitUtterance(const int16_t *pcm, size_t samples);  // false if busy/disabled
bool brainSubmitEvent(const char *eventName);                   // false if busy/disabled
bool brainTakeResult(BrainResult *out);   // true once when DONE; back to IDLE
const int16_t *brainReplyPcm(size_t *samples);
void brainPrintStatus();                  // for the debug console

#endif
