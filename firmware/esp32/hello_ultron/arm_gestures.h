/**********************************************************************
  Arm/PCA9685 gestures - DEFERRED from Mini-Ultron v1.

  v1 direction is OLED face + neck + voice + wheels; no arm servos. This
  module is preserved, known-working PCA9685 code from earlier prototyping
  (2x arm servos, channels 0/1, center = 1500us). It is compiled out of the
  active v1 path by default so nothing in normal operation depends on the
  PCA9685/I2C bus.

  Flip MINI_ULTRON_ENABLE_ARMS to 1 in features.h to bring PCA9685 arm
  gestures back into the active build.
**********************************************************************/
#ifndef ARM_GESTURES_H
#define ARM_GESTURES_H

#include "features.h"

#if MINI_ULTRON_ENABLE_ARMS
void armsInit();
void handsNeutral();
void handsUp();
void leftHandOut();
void rightHandOut();
void waveRightHand();
#endif

#endif
