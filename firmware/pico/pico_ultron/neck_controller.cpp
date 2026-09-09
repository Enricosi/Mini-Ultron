#include <Arduino.h>
#include "Freenove_4WD_Car_For_Pico_W.h"
#include "neck_controller.h"

// Angle constants, in degrees, passed to the vendored Servo_1_Angle().
//
// Freenove's own driver/demo (01.2_Servo) treats 30-150 degrees as the
// valid/supported range and sweeps through the full range on their own
// reference hardware, so that range itself is vendor-supported behavior,
// not a guess. The LEFT/RIGHT values below are a conservative subset of
// that range (not the 30/150 extremes) because Mini-Ultron's neck
// bracket/mount is Enrico's own installation, not necessarily identical
// to Freenove's reference bracket.
//
// NEEDS_PHYSICAL_VERIFICATION: confirm these angles don't bind or strain
// the actual mount before trusting them further; widen or narrow once
// verified on the real hardware.
static const float NECK_ANGLE_CENTER = 90.0f;
static const float NECK_ANGLE_LEFT   = 60.0f;
static const float NECK_ANGLE_RIGHT  = 120.0f;

static NeckPosition currentPosition = NECK_CENTER;

void neckControllerInit() {
  Servo_Setup();
  neckSetPosition(NECK_CENTER);  // safe, centered startup position
}

void neckSetPosition(NeckPosition pos) {
  switch (pos) {
    case NECK_LEFT:
      Servo_1_Angle(NECK_ANGLE_LEFT);
      currentPosition = NECK_LEFT;
      break;
    case NECK_RIGHT:
      Servo_1_Angle(NECK_ANGLE_RIGHT);
      currentPosition = NECK_RIGHT;
      break;
    case NECK_CENTER:
    default:
      Servo_1_Angle(NECK_ANGLE_CENTER);
      currentPosition = NECK_CENTER;
      break;
  }
}

NeckPosition neckGetPosition() {
  return currentPosition;
}
