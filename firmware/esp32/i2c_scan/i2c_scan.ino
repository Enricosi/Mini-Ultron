#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(1, 48);
}

void loop() {
    for (int address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        int error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("I2C device found at 0x");

            if (address < 16) {
                Serial.print("0");
            }

            Serial.println(address, HEX);
        }
    }

    delay(3000);
}
