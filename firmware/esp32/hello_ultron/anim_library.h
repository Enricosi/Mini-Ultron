/**********************************************************************
  Built-in (PROGMEM) face animations. Few and small: each one costs app
  flash, which is already ~85% used. Bigger/more animations belong on
  LittleFS instead (MINI_ULTRON_ENABLE_ANIM_FS, see anim_player.h).

  Adding one: run tools/anim/anim_convert.py with --format h into
  src/animations/, then add its #include and one line to the table in
  anim_library.cpp.
**********************************************************************/
#ifndef ANIM_LIBRARY_H
#define ANIM_LIBRARY_H

#include <Arduino.h>

struct BuiltinAnim {
  const char *name;
  const uint8_t *data;  // whole .ula blob, in flash
  size_t size;
};

extern const BuiltinAnim BUILTIN_ANIMS[];
extern const size_t BUILTIN_ANIM_COUNT;

#endif
