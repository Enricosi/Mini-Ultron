/**********************************************************************
  Deterministic neck/front-servo control. The servo is physically wired
  to the Freenove car shield's own servo header (see chassis_hal.h), so
  it belongs to the Pico. Only named, pre-validated positions are
  exposed above this layer - never a raw angle or pulse width.
**********************************************************************/
#ifndef NECK_CONTROLLER_H
#define NECK_CONTROLLER_H

enum NeckPosition {
  NECK_CENTER,
  NECK_LEFT,
  NECK_RIGHT
};

void neckControllerInit();               // moves to the safe startup position (center)
void neckSetPosition(NeckPosition pos);  // the only way to move the neck
NeckPosition neckGetPosition();

#endif
