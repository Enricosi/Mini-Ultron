/**********************************************************************
  Hardware abstraction for Mini-Ultron's chassis: the Freenove FNK0089
  4WD Mecanum kit's 4 DC motors, the front/neck servo on the shield's
  own header, the HC-SR04 ultrasonic sensor, and the 3-channel IR line
  tracker.

  Written from scratch for Mini-Ultron - NOT vendored from Freenove's
  official example sketches (which this file replaces:
  Freenove_4WD_Car_For_Pico_W.{h,cpp}, removed). Freenove's own
  reference repo (freenove-car-reference/, gitignored locally) ships no
  top-level LICENSE file, which was a real open-source/redistribution
  risk (see THIRD_PARTY_LICENSES.md) as long as Mini-Ultron's own
  history contained a copy of their code. Pin numbers, motor polarity,
  and the servo's angle-to-duty-cycle mapping below are physically
  verified facts about this specific chassis (not copyrightable
  expression) - see docs/CURRENT_STATE.md. Changing any of them means
  re-testing on hardware, not just recompiling.

  Requires the third-party "RP2040_PWM" library (khoih-prog, MIT) via
  the Arduino Library Manager - the same dependency the old vendored
  driver used to generate PWM; unrelated to Freenove's own licensing
  question.
**********************************************************************/
#ifndef CHASSIS_HAL_H
#define CHASSIS_HAL_H

#include <Arduino.h>

// ---- Neck/front servo (Freenove shield's own servo header) ----
void chassisServoInit();
void chassisServoSetAngle(float degrees);  // clamped to 30-150 degrees

// ---- Drive motors (Mecanum, one H-bridge pin pair per wheel) ----
// M1=front-left M2=back-left M3=back-right M4=front-right, matching the
// physically-verified wheel-position mapping in motor_controller.h.
#define CHASSIS_SPEED_MIN -100
#define CHASSIS_SPEED_MAX  100

void chassisMotorsInit();
void chassisSetWheelSpeeds(int m1, int m2, int m3, int m4);  // each CHASSIS_SPEED_MIN..MAX

// ---- Ultrasonic (HC-SR04) ----
#define CHASSIS_SONIC_MAX_CM 300

void chassisUltrasonicInit();
float chassisReadDistanceCm();  // CHASSIS_SONIC_MAX_CM if no echo before timeout

// ---- 3-channel IR line tracker ----
void chassisTrackInit();
void chassisReadTrack(bool out[3]);  // [left, center, right]; true = line detected

#endif
