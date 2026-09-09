/**********************************************************************
  OLED face - Mini-Ultron's primary expressive channel. SSD1331 96x64
  full-color display over software SPI (wiring is fixed, do not change
  without updating docs/CURRENT_STATE.md).

  All animation is millis()-driven (non-blocking) so it can coexist with
  UART/network/audio work later. Call faceUpdate() every loop() iteration.
**********************************************************************/
#ifndef FACE_H
#define FACE_H

#include <Arduino.h>

enum FaceState {
  FACE_IDLE,        // resting "evil stare" - default state
  FACE_ANGRY,       // heightened aggressive reaction
  FACE_LISTENING,   // static, attentive
  FACE_THINKING,    // pulsing, processing
  FACE_SPEAKING,    // talking waveform
  FACE_SURPRISED,   // static, wide eyes
  FACE_SUSPICIOUS,  // static, squint
  FACE_SLEEPING,    // slow breathing pulse
  FACE_GLITCH       // error / malfunction
};

void faceInit();                     // begins the display, draws the boot state
void setFace(FaceState newState);    // switches expressive state, redraws immediately
FaceState getFace();
void faceUpdate(unsigned long now);  // call every loop() iteration - non-blocking

#endif
