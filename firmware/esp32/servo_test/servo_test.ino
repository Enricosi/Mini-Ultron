#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40, Wire);

void setup() {Serial.begin(115200);

Wire.begin(1, 48);

pwm.begin();

pwm.setPWMFreq(50);

pwm.writeMicroseconds(0, 1500);
}

void loop() {
    pwm.writeMicroseconds(0, 1100);
    delay(1000);

    pwm.writeMicroseconds(0, 1500);
    delay(1000);

    pwm.writeMicroseconds(0, 1900);
    delay(1000);

    pwm.writeMicroseconds(0, 1500);
    delay(1000);
}
