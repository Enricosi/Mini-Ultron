/**********************************************************************
  Car-shield WS2812 status LEDs (8 LEDs, GPIO16 - Freenove FNK0089).

  Not part of the vendored Freenove_4WD_Car_For_Pico_W driver on purpose:
  that file's header explicitly lists WS2812 among the sections left out
  (not needed until now). Rather than pull in Freenove's own WS2812
  helper file (flow/blink/rainbow modes we don't need), this is a small
  from-scratch module in Mini-Ultron's own style: a fixed, named-state
  API only, like neck_controller.h - no raw RGB/animation surface is
  exposed above this layer.

  Purely cosmetic/status indication. Never gates or reacts to motor,
  neck, or obstacle state - safety logic must not depend on these LEDs.
**********************************************************************/
#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

enum LedState {
  LED_OFF,
  LED_READY  // dim violet - boot/idle indicator
};

void ledControllerInit();          // sets up the strip and turns on LED_READY
void ledSetState(LedState state);  // the only way to change the LEDs
LedState ledGetState();

#endif
