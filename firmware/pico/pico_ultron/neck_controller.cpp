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
// Physically tested 2026-09-13: the original assignment (LEFT=60,
// RIGHT=120) was mirrored on this mount - swapped below to match what
// Enrico actually observed. That swap is correct and stays.
//
// A same-day +10 deg software offset attempt at re-centering (tried
// because 90 deg visibly sat ~10-15 deg right of true center) did NOT
// fix it - reverted. Freenove's own assembly manual
// (freenove-car-reference/Mecanum_wheels/Tutorial.pdf, p.51, "Mounting
// the Servo Arm to Module Connector") says the rocker arm/horn must be
// physically attached to the servo spline *while the servo is actively
// holding 90 deg* - that's what makes 90 deg equal true mechanical
// center. A software offset fights a mechanical misalignment instead of
// fixing it (doesn't address binding/asymmetric strain at the extremes),
// so this is reverted to the vendor-clean 90/60/120 rather than another
// guessed constant. The real fix is mechanical: loosen the horn's
// center screw, hold NECK:CENTER (90 deg) via this firmware, re-seat the
// horn aligned to the chassis, retighten. See CURRENT_STATE.md.
//
// NEEDS_PHYSICAL_VERIFICATION (still open): confirm center is correct
// after the horn is re-seated, and that LEFT/RIGHT don't bind or strain
// the mount.
static const float NECK_ANGLE_CENTER = 90.0f;
static const float NECK_ANGLE_LEFT   = 120.0f;
static const float NECK_ANGLE_RIGHT  = 60.0f;

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
