/**********************************************************************
  High-level, deterministic interface to the Pico W locomotion/neck/
  safety controller. This is the ONLY place in the ESP32 firmware that
  knows the Pico's ASCII wire protocol - everything else calls these
  functions.

  Speeds are validated (0-100) before anything is sent. There is no
  function here that accepts an arbitrary/raw command string, so this
  interface cannot be used to send arbitrary PWM/servo/GPIO values to the
  Pico - only the fixed vocabulary the Pico's command_parser understands.

  See pico_link_config.h for transport status: until ESP32 UART pins are
  chosen, these calls log instead of transmitting.
**********************************************************************/
#ifndef PICO_LINK_H
#define PICO_LINK_H

#include <Arduino.h>

void picoLinkInit();
void picoLinkPoll();  // call every loop() iteration - non-blocking

bool picoLinkIsConnected();  // true once a real UART transport is configured

// --- locomotion (mirrors the Pico's MOVE/ROTATE/STOP protocol) ---
void picoPing();
void picoStop();
void picoStatus();
void picoMoveForward(int speedPercent);
void picoMoveBackward(int speedPercent);
void picoMoveLeft(int speedPercent);   // strafe
void picoMoveRight(int speedPercent);  // strafe
void picoRotateLeft(int speedPercent);
void picoRotateRight(int speedPercent);

// --- neck (mirrors the Pico's NECK protocol) ---
void picoNeckCenter();
void picoNeckLeft();
void picoNeckRight();

// --- car-shield WS2812 status LEDs (mirrors the Pico's LED protocol) ---
void picoLedOn();
void picoLedOff();
// Mirrors the OLED face on the shield's WS2812 LEDs. faceName must be one of
// the 9 FaceState names (face.h) - anything else is dropped, never forwarded
// raw, matching the Pico's fixed LED:FACE:<NAME> vocabulary.
void picoLedFace(const char *faceName);

// --- last known state, updated by picoLinkPoll() from Pico responses ---
enum PicoResponseType {
  PICO_RESP_NONE,
  PICO_RESP_READY,
  PICO_RESP_OK,
  PICO_RESP_ERR,
  PICO_RESP_OBSTACLE,
  PICO_RESP_STOPPED_TIMEOUT,
  PICO_RESP_DIST,
  PICO_RESP_LINE,
  PICO_RESP_UNKNOWN
};

PicoResponseType picoLastResponseType();
float picoLastDistanceCm();      // valid after a PICO_RESP_DIST
const char *picoLastLineBits();  // valid after a PICO_RESP_LINE, e.g. "010"
bool picoIsAlive();              // true once at least one response has been seen

#endif
