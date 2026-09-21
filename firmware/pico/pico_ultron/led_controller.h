/**********************************************************************
  Car-shield WS2812 status LEDs (8 LEDs, GPIO16 - Freenove FNK0089).

  Not part of the vendored Freenove_4WD_Car_For_Pico_W driver on purpose:
  that file's header explicitly lists WS2812 among the sections left out
  (not needed until now). Rather than pull in Freenove's own WS2812
  helper file (flow/blink/rainbow modes we don't need), this is a small
  from-scratch module in Mini-Ultron's own style: a fixed, named-state
  API only, like neck_controller.h - no raw RGB/animation surface is
  exposed above this layer.

  States mirror the ESP32's FaceState vocabulary (face.h) so the shield
  LEDs can show the same "emotion" as the OLED - LED:FACE:<NAME> over the
  wire, same names, one color per state, no per-pixel/animation control.

  Purely cosmetic/status indication. Never gates or reacts to motor,
  neck, or obstacle state - safety logic must not depend on these LEDs.
**********************************************************************/
#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>

enum LedState {
  LED_OFF,
  LED_IDLE,        // dim violet - boot/idle indicator, mirrors FACE_IDLE
  LED_ANGRY,       // dim red
  LED_LISTENING,   // dim cyan
  LED_THINKING,    // dim blue
  LED_SPEAKING,    // dim green
  LED_SURPRISED,   // dim white
  LED_SUSPICIOUS,  // dim orange
  LED_SLEEPING,    // very dim blue
  LED_GLITCH       // dim magenta
};

void ledControllerInit();          // sets up the strip and turns on LED_IDLE
void ledSetState(LedState state);  // the only way to change the LEDs
LedState ledGetState();

// Maps a face-state name (exact match, e.g. "ANGRY") to a LedState. Returns
// false and leaves *out untouched for anything outside the known vocabulary -
// callers must treat that as ERR:PARSE, not a silent no-op.
bool ledStateForFaceName(const String &name, LedState *out);

#endif
