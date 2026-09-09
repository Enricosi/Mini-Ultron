/**********************************************************************
  Mini-Ultron high-level locomotion API, built on top of Freenove's
  Motor_M_Move(M1,M2,M3,M4). Wheel order (verified against the official
  01.1_Car_Move_and_Turn demo):
    M1 = front-left   M2 = back-left
    M3 = front-right  M4 = back-right
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
