// Standalone MPU6050 bench test (ESP32-S3). Raw Wire, no external library.
// Wiring: VCC->5V (module has onboard regulator), GND->GND, SDA->GPIO13, SCL->GPIO14.
// NOTE: this board reports WHO_AM_I=0x70 (MPU6500-family), not 0x68. Accel/gyro registers
// are compatible; the temperature scale differs, so the temp value is only indicative.
// Prints WHO_AM_I once, then accel (g) / gyro (deg/s) / temp (C) 5x per second.
// delay() is fine here: isolated bench sketch, not the robot loop.
#include <Wire.h>

const int MPU_SDA = 13;
const int MPU_SCL = 14;
const uint8_t MPU_ADDR = 0x68;  // 0x69 if AD0 is pulled high

const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_WHO_AM_I = 0x75;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;

// Default ranges after wake-up: +/-2g (16384 LSB/g), +/-250 deg/s (131 LSB/deg/s)
const float ACCEL_LSB_PER_G = 16384.0;
const float GYRO_LSB_PER_DPS = 131.0;

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // let the USB CDC console attach
  Wire.begin(MPU_SDA, MPU_SCL);

  uint8_t who = 0;
  if (!readRegs(REG_WHO_AM_I, &who, 1)) {
    Serial.println("MPU6050 not responding (check wiring / address)");
  } else {
    Serial.printf("WHO_AM_I = 0x%02X (0x68 = MPU6050, 0x70 = MPU6500 clone; both read the same here)\n", who);
  }

  // Chip powers up asleep; clear the sleep bit.
  if (!writeReg(REG_PWR_MGMT_1, 0x00)) {
    Serial.println("wake-up write failed");
  }
  delay(100);
}

void loop() {
  uint8_t raw[14];
  if (!readRegs(REG_ACCEL_XOUT_H, raw, sizeof(raw))) {
    Serial.println("read failed");
    delay(1000);
    return;
  }

  int16_t ax = (raw[0] << 8) | raw[1];
  int16_t ay = (raw[2] << 8) | raw[3];
  int16_t az = (raw[4] << 8) | raw[5];
  int16_t t = (raw[6] << 8) | raw[7];
  int16_t gx = (raw[8] << 8) | raw[9];
  int16_t gy = (raw[10] << 8) | raw[11];
  int16_t gz = (raw[12] << 8) | raw[13];

  Serial.printf("acc[g] %6.2f %6.2f %6.2f | gyro[dps] %7.1f %7.1f %7.1f | %.1fC\n",
                ax / ACCEL_LSB_PER_G, ay / ACCEL_LSB_PER_G, az / ACCEL_LSB_PER_G,
                gx / GYRO_LSB_PER_DPS, gy / GYRO_LSB_PER_DPS, gz / GYRO_LSB_PER_DPS,
                t / 340.0 + 36.53);
  delay(200);
}
