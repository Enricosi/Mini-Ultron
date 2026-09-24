#include <Arduino.h>
#include "chassis_hal.h"
#include "motor_controller.h"

static bool isMoving = false;

void motorControllerInit() {
  stopCar();
}

bool motorIsMoving() {
  return isMoving;
}

void moveForward(int speed) {
  speed = constrain(speed, 0, CHASSIS_SPEED_MAX);
  chassisSetWheelSpeeds(speed, speed, speed, speed);
  isMoving = (speed != 0);
}

void moveBackward(int speed) {
  speed = constrain(speed, 0, CHASSIS_SPEED_MAX);
  chassisSetWheelSpeeds(-speed, -speed, -speed, -speed);
  isMoving = (speed != 0);
}

void strafeLeft(int speed) {
  speed = constrain(speed, 0, CHASSIS_SPEED_MAX);
  chassisSetWheelSpeeds(-speed, speed, -speed, speed);
  isMoving = (speed != 0);
}

void strafeRight(int speed) {
  speed = constrain(speed, 0, CHASSIS_SPEED_MAX);
  chassisSetWheelSpeeds(speed, -speed, speed, -speed);
  isMoving = (speed != 0);
}

void rotateLeft(int speed) {
  speed = constrain(speed, 0, CHASSIS_SPEED_MAX);
  chassisSetWheelSpeeds(-speed, -speed, speed, speed);
  isMoving = (speed != 0);
}

void rotateRight(int speed) {
  speed = constrain(speed, 0, CHASSIS_SPEED_MAX);
  chassisSetWheelSpeeds(speed, speed, -speed, -speed);
  isMoving = (speed != 0);
}

// Derived from superposing forward/backward with strafe-left/right
// (see write-up: matches 3 of the 4 combinations in Freenove's own
// demo .ino; the "forward-left" one in that demo was a copy/paste
// duplicate of "strafe left" and is not used as a reference here).
void moveDiagonal(DiagonalDirection dir, int speed) {
  speed = constrain(speed, 0, CHASSIS_SPEED_MAX);
  switch (dir) {
    case DIAG_FORWARD_RIGHT:
      chassisSetWheelSpeeds(speed, 0, speed, 0);
      break;
    case DIAG_FORWARD_LEFT:
      chassisSetWheelSpeeds(0, speed, 0, speed);
      break;
    case DIAG_BACKWARD_RIGHT:
      chassisSetWheelSpeeds(0, -speed, 0, -speed);
      break;
    case DIAG_BACKWARD_LEFT:
      chassisSetWheelSpeeds(-speed, 0, -speed, 0);
      break;
  }
  isMoving = (speed != 0);
}

void stopCar() {
  chassisSetWheelSpeeds(0, 0, 0, 0);
  isMoving = false;
}
