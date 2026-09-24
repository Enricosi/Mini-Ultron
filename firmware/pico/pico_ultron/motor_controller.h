/**********************************************************************
  Mini-Ultron high-level locomotion API, built on top of
  chassisSetWheelSpeeds(M1,M2,M3,M4) (see chassis_hal.h). Wheel order
  (physically verified on this chassis by Enrico, 2026-09-13 —
  supersedes the earlier inference from Freenove's demo code, which had
  M3/M4 swapped):
    M1 = front-left   M2 = back-left
    M3 = back-right   M4 = front-right
**********************************************************************/
#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

// Diagonal directions, kept ready for a future protocol command.
enum DiagonalDirection {
  DIAG_FORWARD_LEFT,
  DIAG_FORWARD_RIGHT,
  DIAG_BACKWARD_LEFT,
  DIAG_BACKWARD_RIGHT
};

void motorControllerInit();

void moveForward(int speed);   // speed: 0-100
void moveBackward(int speed);  // speed: 0-100
void strafeLeft(int speed);    // speed: 0-100
void strafeRight(int speed);   // speed: 0-100
void rotateLeft(int speed);    // speed: 0-100
void rotateRight(int speed);   // speed: 0-100
void moveDiagonal(DiagonalDirection dir, int speed);  // speed: 0-100
void stopCar();

bool motorIsMoving();  // false right after stopCar(), true after any move* call

#endif
