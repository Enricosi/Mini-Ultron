#include "chassis_hal.h"
#include "RP2040_PWM.h"

// -----------------------------------------------------------------------
// Neck/front servo
// -----------------------------------------------------------------------
static const uint32_t kServoPin = 13;
static const float kServoFreqHz = 50.0f;
static RP2040_PWM* servoPwm = nullptr;

void chassisServoInit() {
  servoPwm = new RP2040_PWM(kServoPin, kServoFreqHz, 0.0f);
  if (servoPwm) servoPwm->setPWM();
}

void chassisServoSetAngle(float degrees) {
  degrees = constrain(degrees, 30.0f, 150.0f);
  // Physically-calibrated angle -> duty-cycle-percent mapping (see the
  // 60/90/120 constants tuned on this chassis in neck_controller.cpp).
  // RP2040_PWM::setPWM() takes duty cycle as a percentage of the period.
  float dutyPercent = map(degrees, 0.0f, 180.0f, 2500.0f, 12500.0f) / 1000.0f;
  if (servoPwm) servoPwm->setPWM(kServoPin, kServoFreqHz, dutyPercent);
}

// -----------------------------------------------------------------------
// Drive motors - one H-bridge pin pair per wheel: PWM the pin for the
// direction being driven, hold the other pin's duty cycle at 0.
// -----------------------------------------------------------------------
struct MotorWiring {
  uint32_t forwardPin;
  uint32_t reversePin;
  bool inverted;  // physically-verified wiring polarity, see CURRENT_STATE.md
};

// M1=front-left M2=back-left M3=back-right M4=front-right
static const MotorWiring kMotors[4] = {
  { 18, 19, true },  // M1
  { 21, 20, true },  // M2
  { 7,  6,  true },  // M3
  { 9,  8,  true },  // M4
};
static const float kMotorFreqHz = 500.0f;
static RP2040_PWM* motorForwardPwm[4] = {};
static RP2040_PWM* motorReversePwm[4] = {};

void chassisMotorsInit() {
  for (uint8_t i = 0; i < 4; i++) {
    motorForwardPwm[i] = new RP2040_PWM(kMotors[i].forwardPin, kMotorFreqHz, 0.0f);
    motorReversePwm[i] = new RP2040_PWM(kMotors[i].reversePin, kMotorFreqHz, 0.0f);
    if (motorForwardPwm[i]) motorForwardPwm[i]->setPWM();
    if (motorReversePwm[i]) motorReversePwm[i]->setPWM();
  }
}

static void driveMotor(uint8_t index, int speed) {
  speed = constrain(speed, CHASSIS_SPEED_MIN, CHASSIS_SPEED_MAX);
  if (kMotors[index].inverted) speed = -speed;

  RP2040_PWM* forward = motorForwardPwm[index];
  RP2040_PWM* reverse = motorReversePwm[index];
  if (speed >= 0) {
    if (forward) forward->setPWM(kMotors[index].forwardPin, kMotorFreqHz, speed);
    if (reverse) reverse->setPWM(kMotors[index].reversePin, kMotorFreqHz, 0);
  } else {
    if (reverse) reverse->setPWM(kMotors[index].reversePin, kMotorFreqHz, -speed);
    if (forward) forward->setPWM(kMotors[index].forwardPin, kMotorFreqHz, 0);
  }
}

void chassisSetWheelSpeeds(int m1, int m2, int m3, int m4) {
  driveMotor(0, m1);
  driveMotor(1, m2);
  driveMotor(2, m3);
  driveMotor(3, m4);
}

// -----------------------------------------------------------------------
// Ultrasonic (HC-SR04) - standard trigger/echo pulse ranging.
// -----------------------------------------------------------------------
static const uint8_t kSonicTrigPin = 4;
static const uint8_t kSonicEchoPin = 5;
static const float kSoundVelocityCmPerUs = 340.0f / 10000.0f;  // 340 m/s
static const unsigned long kSonicTimeoutUs = (unsigned long)CHASSIS_SONIC_MAX_CM * 60UL;

void chassisUltrasonicInit() {
  pinMode(kSonicTrigPin, OUTPUT);
  pinMode(kSonicEchoPin, INPUT);
}

float chassisReadDistanceCm() {
  digitalWrite(kSonicTrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(kSonicTrigPin, LOW);

  unsigned long echoUs = pulseIn(kSonicEchoPin, HIGH, kSonicTimeoutUs);
  if (echoUs == 0) return CHASSIS_SONIC_MAX_CM;  // no echo before timeout
  return echoUs * kSoundVelocityCmPerUs / 2.0f;   // round-trip time -> one-way distance
}

// -----------------------------------------------------------------------
// 3-channel IR line tracker - plain digital reads.
// -----------------------------------------------------------------------
static const uint8_t kTrackLeftPin = 12;
static const uint8_t kTrackCenterPin = 11;
static const uint8_t kTrackRightPin = 10;

void chassisTrackInit() {
  pinMode(kTrackLeftPin, INPUT);
  pinMode(kTrackCenterPin, INPUT);
  pinMode(kTrackRightPin, INPUT);
}

void chassisReadTrack(bool out[3]) {
  out[0] = digitalRead(kTrackLeftPin) != 0;
  out[1] = digitalRead(kTrackCenterPin) != 0;
  out[2] = digitalRead(kTrackRightPin) != 0;
}
