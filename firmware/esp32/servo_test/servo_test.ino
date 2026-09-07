#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40, Wire);
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

void setup() {
    Serial.begin(115200);

    Wire.begin(1, 48);

    pwm.begin();
    pwm.setPWMFreq(50);

    pwm.writeMicroseconds(0, 1500);
    pwm.writeMicroseconds(1, 1500);
    
    handsNeutral();
}

void loop() {
    if (Serial.available() > 0) {
        char command = Serial.read();

        if (command == 'n') {
            handsNeutral();
        }

        if (command == 'u') {
            handsUp();
        }

        if (command == 'l') {
            leftHandOut();
        }

        if (command == 'r') {
            rightHandOut();
        }

        if (command == 'w') {
            waveRightHand();
        }
    }
}
