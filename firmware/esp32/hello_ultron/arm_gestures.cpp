#include "arm_gestures.h"

#if MINI_ULTRON_ENABLE_ARMS

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

static Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40, Wire);

void armsInit() {
    Wire.begin(1, 48);
    pwm.begin();
    pwm.setPWMFreq(50);
    handsNeutral();
}

void handsNeutral() {
    pwm.writeMicroseconds(0, 1500);
    pwm.writeMicroseconds(1, 1500);
}

void handsUp() {
    pwm.writeMicroseconds(0, 2000);
    pwm.writeMicroseconds(1, 1000);
}

void leftHandOut() {
    pwm.writeMicroseconds(0, 1300);
    pwm.writeMicroseconds(1, 1500);
}

void rightHandOut() {
    pwm.writeMicroseconds(0, 1500);
    pwm.writeMicroseconds(1, 1700);
}

void waveRightHand() {
    pwm.writeMicroseconds(1, 1700);
    delay(250);

    pwm.writeMicroseconds(1, 1400);
    delay(250);

    pwm.writeMicroseconds(1, 1700);
    delay(250);

    pwm.writeMicroseconds(1, 1400);
    delay(250);

    pwm.writeMicroseconds(1, 1500);
}

#endif  // MINI_ULTRON_ENABLE_ARMS
