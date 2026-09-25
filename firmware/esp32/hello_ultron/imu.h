/**********************************************************************
  IMU - MPU6050-family accelerometer + gyro over I2C (SDA=GPIO13,
  SCL=GPIO14, see docs/CURRENT_STATE.md). The physical board reports
  WHO_AM_I=0x70 (MPU6500-family clone); accel/gyro registers are
  compatible, temperature is deliberately not exposed.

  Shocks are only RECORDED (joltCount / lastJoltMs), never acted on here: nothing
  reacts in real time. Django can read them later as context ("I was shaken 3 s
  ago"). Because the I2S audio clocks (GPIO9/10) corrupt some I2C reads on this
  wiring (see docs/CURRENT_STATE.md), a jolt needs 2 consecutive samples over the
  threshold and all-zero accel reads are discarded.

  Non-blocking: call imuUpdate(now) every loop() iteration; it samples
  every 10 ms. The gyro bias is calibrated without blocking (~1 s, robot
  must be still - a calibration that sees motion is rejected and the
  previous bias kept). Ranges: accel +/-4 g, gyro +/-500 dps, DLPF ~44 Hz.

  Note: arm_gestures.cpp (compiled out, MINI_ULTRON_ENABLE_ARMS 0) calls
  Wire.begin() on other pins; do not enable it alongside this module.

  Disabled with MINI_ULTRON_ENABLE_IMU 0 (features.h, e.g. a build with no IMU
  wired): every function below still exists but becomes a harmless stub
  (imuGetData().ok stays false, no jolt ever fires), so callers - including
  conversation.cpp's jolt reaction - need no #if of their own.
**********************************************************************/
#ifndef IMU_H
#define IMU_H

#include <Arduino.h>
#include "features.h"

struct ImuData {
  float ax, ay, az;      // g
  float gx, gy, gz;      // deg/s, bias-corrected
  float rollDeg;         // rotation about the X axis, from gravity
  float pitchDeg;        // rotation about the Y axis, from gravity
  bool ok;               // last read succeeded
  bool calibrated;       // gyro bias measured with the robot still
  bool calibrating;
  uint32_t joltCount;    // sudden shocks since boot (|accel magnitude - 1 g| > threshold)
  unsigned long lastJoltMs;  // millis() of the last jolt, 0 = none yet
  uint32_t readsOk;      // diagnostics: I2C reads that succeeded / failed since boot
  uint32_t readsFailed;
};

void imuInit();
void imuUpdate(unsigned long now);  // call every loop() iteration - non-blocking
ImuData imuGetData();
void imuStartCalibration();         // keep the robot still for ~1 s

#endif
