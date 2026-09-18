#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(13, 14);  // MPU6050 bus: SDA=GPIO13, SCL=GPIO14
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
