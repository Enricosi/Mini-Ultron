/**********************************************************************
  Mini-Ultron (Pico W) optional subsystems - the one place to turn a
  physical component on or off for THIS build, same contract as the
  ESP32's hello_ultron/features.h: 0 = not installed, its code stays in
  the tree and the rest of the firmware behaves sensibly without it.

  #ifndef guards mean a -D flag on the compile command can still override
  a default here without editing this file.
**********************************************************************/
#ifndef FEATURES_H
#define FEATURES_H

// HC-SR04 ultrasonic sensor (TRIG=GP4, ECHO=GP5). Not part of the v1 build
// (removed 2026-10-02). When 0: its pins are never configured, STATUS
// reports DIST:-1 ("no sensor"), and MOVE:FWD has NO obstacle veto - the
// remaining guards are the speed clamp, the comms watchdog, STOP priority,
// and the ESP32's fixed slow time-bounded bursts.
#ifndef MINI_ULTRON_ENABLE_ULTRASONIC
#define MINI_ULTRON_ENABLE_ULTRASONIC 0
#endif

#endif
