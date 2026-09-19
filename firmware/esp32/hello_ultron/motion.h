/**********************************************************************
  Turns an ABSTRACT movement intent from CEREBRO into a short, bounded burst of
  the Pico's existing protocol commands. The LLM/Django never choose speeds or
  durations: they are fixed constants in motion.cpp.

  The Pico's watchdog stops the car ~500 ms after the last command, so this
  module re-sends the command every 200 ms for a fixed duration and then sends
  STOP itself. If the Pico reports OBSTACLE or a timeout, motion ends at once.
  Everything is millis()-based; call motionUpdate() every loop() iteration.

  Disabled unless MINI_ULTRON_BRAIN_MOVEMENT is 1 (brain_config.h): with it at 0
  every intent is logged and ignored.
**********************************************************************/
#ifndef MOTION_H
#define MOTION_H

#include <Arduino.h>

enum MotionIntent {
  MOTION_NONE,
  MOTION_FORWARD,
  MOTION_BACKWARD,
  MOTION_STRAFE_LEFT,
  MOTION_STRAFE_RIGHT,
  MOTION_ROTATE_LEFT,
  MOTION_ROTATE_RIGHT
};

MotionIntent motionFromName(const char *name);  // exact CEREBRO names; unknown -> MOTION_NONE
void motionStart(MotionIntent intent);
void motionAbort();                              // sends STOP if a burst is running
void motionUpdate(unsigned long now);
bool motionActive();

#endif
