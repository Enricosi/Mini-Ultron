#include <Arduino.h>
#include "safety_controller.h"
#include "motor_controller.h"

static unsigned long lastCommandMillis = 0;

void safetyInit() {
  stopCar();  // boot state: motors are always off before anything else runs
  lastCommandMillis = millis();
}

void safetyNoteCommandReceived() {
  lastCommandMillis = millis();
}

bool safetyCheckTimeout() {
  if (motorIsMoving() && (millis() - lastCommandMillis > COMM_TIMEOUT_MS)) {
    stopCar();  // motorIsMoving() becomes false, so this fires only once per timeout
    return true;
  }
  return false;
}

bool safetyForwardBlocked(float distanceCm) {
  return distanceCm < OBSTACLE_STOP_CM;
}
