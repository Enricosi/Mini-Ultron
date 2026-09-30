/**********************************************************************
  .ula - Mini-Ultron 1-bit face animation format. The same bytes are used
  whether the animation is compiled in (PROGMEM) or stored on LittleFS, so
  there is exactly one parser. Produced by tools/anim/anim_convert.py - keep
  both in sync if the layout ever changes (bump the magic when it does).

  All fields little-endian:
    0       4           magic "ULA1"
    4       2           width  (px, <= ANIM_MAX_W)
    6       2           height (px, <= ANIM_MAX_H)
    8       2           frame count N (>= 1)
    10      2           reserved
    12      2           RGB565 color for "on" pixels
    14      2           reserved
    16      2*N         per-frame delay, ms
    16+2N   N*frameBytes frames: row-major, ceil(W/8) bytes per row,
                        MSB = leftmost pixel, 1 = on
**********************************************************************/
#ifndef ANIM_FORMAT_H
#define ANIM_FORMAT_H

#include <Arduino.h>

// Largest animation the player will accept = the SSD1331's full screen.
// Also sizes the one-frame RAM buffers (ANIM_MAX_FRAME_BYTES each).
static const uint16_t ANIM_MAX_W = 96;
static const uint16_t ANIM_MAX_H = 64;
static const size_t ANIM_MAX_FRAME_BYTES = ((ANIM_MAX_W + 7) / 8) * ANIM_MAX_H;

static const size_t ANIM_HEADER_SIZE = 16;

struct AnimHeader {
  uint16_t width;
  uint16_t height;
  uint16_t frameCount;
  uint16_t color;       // RGB565
  size_t rowBytes;      // ceil(width / 8)
  size_t frameBytes;    // rowBytes * height
  size_t delaysOffset;  // = ANIM_HEADER_SIZE
  size_t framesOffset;  // first frame
  size_t totalSize;     // exact expected size of the whole .ula
};

// Parses and validates the first ANIM_HEADER_SIZE bytes. Anything unexpected
// (wrong magic, zero/oversized dimensions, zero frames) returns false.
inline bool animParseHeader(const uint8_t *raw, AnimHeader &out) {
  if (raw[0] != 'U' || raw[1] != 'L' || raw[2] != 'A' || raw[3] != '1') return false;
  out.width = raw[4] | (raw[5] << 8);
  out.height = raw[6] | (raw[7] << 8);
  out.frameCount = raw[8] | (raw[9] << 8);
  out.color = raw[12] | (raw[13] << 8);
  if (out.width == 0 || out.width > ANIM_MAX_W) return false;
  if (out.height == 0 || out.height > ANIM_MAX_H) return false;
  if (out.frameCount == 0) return false;
  out.rowBytes = (out.width + 7) / 8;
  out.frameBytes = out.rowBytes * out.height;
  out.delaysOffset = ANIM_HEADER_SIZE;
  out.framesOffset = ANIM_HEADER_SIZE + 2 * (size_t)out.frameCount;
  out.totalSize = out.framesOffset + out.frameBytes * out.frameCount;
  return true;
}

#endif
