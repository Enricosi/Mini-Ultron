/**********************************************************************
  Face animation player - reads .ula animations (anim_format.h) one frame
  at a time and decides when the next frame is due. It never touches the
  display: face.cpp owns the OLED and draws whatever frame this hands it.

  Sources, looked up by name in this order:
    1. built-in PROGMEM table (anim_library.cpp) - always available
    2. LittleFS "/anim/<name>.ula" - only if MINI_ULTRON_ENABLE_ANIM_FS is 1
       (features.h); uses the default partition table's "spiffs" partition

  RAM cost is one frame buffer (ANIM_MAX_FRAME_BYTES, 768 B) regardless of
  animation length: frames stream from flash/LittleFS as they are shown.
  Non-blocking: call animUpdate() every loop() iteration.
**********************************************************************/
#ifndef ANIM_PLAYER_H
#define ANIM_PLAYER_H

#include <Arduino.h>
#include "anim_format.h"

void animInit();  // mounts LittleFS when enabled; harmless otherwise

// Names are [a-z0-9_], 1-24 chars (same rule as the converter), so a name can
// never escape the /anim/ directory. Returns false (and plays nothing) for an
// unknown name or a file that fails validation.
bool animStart(const char *name, bool loop);
void animStop();
bool animIsPlaying();

// Returns true when a new frame is due and was loaded; *frame then points to
// it (valid until the next call). Returns false otherwise, including once a
// non-looping animation has finished - check animIsPlaying() for that.
bool animUpdate(unsigned long now, const uint8_t **frame);

const AnimHeader &animCurrentHeader();  // meaningful only while playing
void animList(Print &out);              // one "ANIM <name> <source>" line each

#endif
