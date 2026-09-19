#include "motion.h"
#include "brain_config.h"
#include "pico_link.h"

// Fixed, deliberately slow. 40 is the duty cycle verified to move all 4 wheels.
static const int MOTION_SPEED = 40;
static const unsigned long BURST_MS = 800;
static const unsigned long RESEND_MS = 200;        // must stay well under the Pico's 500 ms watchdog
static const unsigned long CHECK_AFTER_MS = 300;   // ignore stale Pico responses at the start

static MotionIntent current = MOTION_NONE;
static unsigned long startMs = 0;
static unsigned long lastSendMs = 0;

MotionIntent motionFromName(const char *name) {
  if (!name) return MOTION_NONE;
  if (!strcmp(name, "FORWARD")) return MOTION_FORWARD;
  if (!strcmp(name, "BACKWARD")) return MOTION_BACKWARD;
  if (!strcmp(name, "STRAFE_LEFT")) return MOTION_STRAFE_LEFT;
  if (!strcmp(name, "STRAFE_RIGHT")) return MOTION_STRAFE_RIGHT;
  if (!strcmp(name, "ROTATE_LEFT")) return MOTION_ROTATE_LEFT;
  if (!strcmp(name, "ROTATE_RIGHT")) return MOTION_ROTATE_RIGHT;
  return MOTION_NONE;
}

static void sendCurrent() {
  switch (current) {
    case MOTION_FORWARD: picoMoveForward(MOTION_SPEED); break;
    case MOTION_BACKWARD: picoMoveBackward(MOTION_SPEED); break;
    case MOTION_STRAFE_LEFT: picoMoveLeft(MOTION_SPEED); break;
    case MOTION_STRAFE_RIGHT: picoMoveRight(MOTION_SPEED); break;
    case MOTION_ROTATE_LEFT: picoRotateLeft(MOTION_SPEED); break;
    case MOTION_ROTATE_RIGHT: picoRotateRight(MOTION_SPEED); break;
    default: break;
  }
}

void motionStart(MotionIntent intent) {
  if (intent == MOTION_NONE) return;
#if MINI_ULTRON_BRAIN_MOVEMENT
  current = intent;
  startMs = lastSendMs = millis();
  sendCurrent();
  Serial.printf("[motion] start intent=%d for %lu ms\n", (int)intent, BURST_MS);
#else
  Serial.printf("[motion] intent %d ignored (MINI_ULTRON_BRAIN_MOVEMENT is 0)\n", (int)intent);
#endif
}

void motionAbort() {
  if (current == MOTION_NONE) return;
  current = MOTION_NONE;
  picoStop();
  Serial.println("[motion] aborted, STOP sent");
}

bool motionActive() {
  return current != MOTION_NONE;
}

void motionUpdate(unsigned long now) {
  if (current == MOTION_NONE) return;

  PicoResponseType last = picoLastResponseType();
  bool pastStart = now - startMs > CHECK_AFTER_MS;
  if (pastStart && (last == PICO_RESP_OBSTACLE || last == PICO_RESP_STOPPED_TIMEOUT)) {
    current = MOTION_NONE;
    picoStop();
    Serial.println("[motion] Pico vetoed/stopped, motion ended");
    return;
  }
  if (now - startMs >= BURST_MS) {
    current = MOTION_NONE;
    picoStop();
    Serial.println("[motion] burst finished, STOP sent");
    return;
  }
  if (now - lastSendMs >= RESEND_MS) {
    lastSendMs = now;
    sendCurrent();
  }
}
