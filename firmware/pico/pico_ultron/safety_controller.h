/**********************************************************************
  Last line of locomotion safety, local to the Pico. The ESP32 (or a
  human over USB) only ever sends high-level intent; this module is
  what actually allows or refuses it.
**********************************************************************/
#ifndef SAFETY_CONTROLLER_H
#define SAFETY_CONTROLLER_H

#define COMM_TIMEOUT_MS     500   // no valid command in this window while moving -> auto STOP
#define OBSTACLE_STOP_CM    15    // forward motion refused below this distance

void safetyInit();

// Call once for every syntactically valid command received (STOP/STATUS/
// PING included) - this is what keeps the communication-timeout alive.
void safetyNoteCommandReceived();

// Call every loop() iteration. Returns true exactly once, the iteration
// it auto-stops the car due to a communication timeout.
bool safetyCheckTimeout();

// Given a fresh distance reading, says whether a forward command must
// be refused right now.
bool safetyForwardBlocked(float distanceCm);

#endif
