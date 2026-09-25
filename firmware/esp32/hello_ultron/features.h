/**********************************************************************
  Mini-Ultron optional subsystems - the one place to turn a physical
  component on or off for THIS build, so the same source tree serves any
  physical Ultron regardless of which parts are actually installed.

  Every entry here follows the same contract used throughout this firmware
  (see CLAUDE.md's "gate it, harmless fallback" rule): 0 means the module
  either compiles out entirely (declarations guarded, call sites must be
  guarded too - see arm_gestures.h) or keeps its API but every function
  becomes a harmless no-op/log-only stub (see brain_link.cpp, imu.cpp) so
  callers need no #if of their own.

  #ifndef guards mean a -D flag on the compile command, or a #define placed
  before #include "features.h" in one .ino/.cpp, can still override a
  default here without editing this file.
**********************************************************************/
#ifndef FEATURES_H
#define FEATURES_H

// Arm/PCA9685 gestures: DEFERRED from v1 hardware (see CLAUDE.md - the PCA9685
// and both arm servos are physically removed from active hardware). Code kept
// as historical reference in arm_gestures.{h,cpp}; call sites are #if-guarded.
#ifndef MINI_ULTRON_ENABLE_ARMS
#define MINI_ULTRON_ENABLE_ARMS 0
#endif

// MPU6050-family IMU: jolt detection (see conversation.cpp) and roll/pitch.
// On by default - v1 firm inclusion. Set to 0 for a build with no IMU wired
// (frees GPIO13/14); imu.{h,cpp} then compiles to harmless stubs, no other
// file needs to change.
#ifndef MINI_ULTRON_ENABLE_IMU
#define MINI_ULTRON_ENABLE_IMU 1
#endif

// Movement intents from CEREBRO (Django). Opt-in even when the brain link
// itself is configured (wifi_config.h present): the robot must be verified
// safe to move before this goes to 1. The Django-side counterpart is
// ULTRON_ALLOW_MOVEMENT (cerebro/.env) - both must be on independently.
#ifndef MINI_ULTRON_BRAIN_MOVEMENT
#define MINI_ULTRON_BRAIN_MOVEMENT 0
#endif

#endif
