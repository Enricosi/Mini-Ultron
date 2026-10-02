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
// Not part of the v1 build (removed 2026-10-02: the I2S audio clocks corrupted
// its I2C reads and caused false jolts). Off = imu.{h,cpp} compile to harmless
// stubs and GPIO13/14 are free; set to 1 for a build that has one wired cleanly.
#ifndef MINI_ULTRON_ENABLE_IMU
#define MINI_ULTRON_ENABLE_IMU 0
#endif

// Face animations stored on LittleFS ("/anim/<name>.ula", see anim_player.h),
// on top of the built-in PROGMEM ones that are always available. Uses the
// default partition table's existing 1.375 MB "spiffs" partition - no
// partition change needed - but costs app flash for the LittleFS code, so
// it stays off until there are more animations than PROGMEM should hold.
#ifndef MINI_ULTRON_ENABLE_ANIM_FS
#define MINI_ULTRON_ENABLE_ANIM_FS 0
#endif

// Movement intents from CEREBRO (Django). Opt-in even when the brain link
// itself is configured (wifi_config.h present): the robot must be verified
// safe to move before this goes to 1. The Django-side counterpart is
// ULTRON_ALLOW_MOVEMENT (cerebro/.env) - both must be on independently.
#ifndef MINI_ULTRON_BRAIN_MOVEMENT
#define MINI_ULTRON_BRAIN_MOVEMENT 0
#endif

#endif
