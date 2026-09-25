#include "imu.h"

#if MINI_ULTRON_ENABLE_IMU

#include <Wire.h>

static const int IMU_SDA = 13;
static const int IMU_SCL = 14;
static const uint8_t IMU_ADDR = 0x68;

static const uint8_t REG_CONFIG = 0x1A;        // DLPF
static const uint8_t REG_GYRO_CONFIG = 0x1B;
static const uint8_t REG_ACCEL_CONFIG = 0x1C;
static const uint8_t REG_ACCEL_XOUT_H = 0x3B;
static const uint8_t REG_PWR_MGMT_1 = 0x6B;
static const uint8_t REG_WHO_AM_I = 0x75;

static const float ACCEL_LSB_PER_G = 8192.0f;   // +/-4 g
static const float GYRO_LSB_PER_DPS = 65.5f;    // +/-500 dps

static const unsigned long SAMPLE_INTERVAL_MS = 10;
static const unsigned long RETRY_INTERVAL_MS = 2000;
static const int MAX_CONSECUTIVE_FAILS = 20;   // 200 ms of nothing = re-initialize
static const int CALIBRATION_SAMPLES = 100;     // ~1 s
static const float CALIBRATION_MAX_RANGE_DPS = 5.0f;  // more spread than this = moving
static const float JOLT_THRESHOLD_G = 0.7f;
static const unsigned long JOLT_COOLDOWN_MS = 2000;
static const int JOLT_CONSECUTIVE_SAMPLES = 2;  // rejects single corrupted reads

static ImuData data;
static int overThresholdCount = 0;
static unsigned long lastSampleMs = 0;
static unsigned long lastRetryMs = 0;
static unsigned long joltCooldownUntil = 0;
static float biasX = 0, biasY = 0, biasZ = 0;
static int consecutiveFails = 0;

// Calibration accumulators (non-blocking, filled one sample per update).
static int calibCount = 0;
static float calibSum[3];
static float calibMin[3], calibMax[3];

static bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool readRegs(uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(IMU_ADDR, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static bool configureSensor() {
  uint8_t who = 0;
  if (!readRegs(REG_WHO_AM_I, &who, 1)) return false;
  return writeReg(REG_PWR_MGMT_1, 0x00) &&   // wake up
         writeReg(REG_CONFIG, 0x03) &&       // DLPF ~44 Hz
         writeReg(REG_GYRO_CONFIG, 0x08) &&  // +/-500 dps
         writeReg(REG_ACCEL_CONFIG, 0x08);   // +/-4 g
}

void imuStartCalibration() {
  calibCount = 0;
  for (int i = 0; i < 3; i++) {
    calibSum[i] = 0;
    calibMin[i] = 1e9f;
    calibMax[i] = -1e9f;
  }
  data.calibrating = true;
}

void imuInit() {
  Wire.begin(IMU_SDA, IMU_SCL);
  // Default 100 kHz on purpose: 400 kHz was not tried again once interference with the
  // I2S clocks (GPIO9/10 next to GPIO13/14) turned out to be the real problem.
  data = ImuData();
  data.ok = configureSensor();
  if (!data.ok) Serial.println("[imu] not responding at boot (will keep retrying)");
  imuStartCalibration();
}

static void finishCalibration() {
  data.calibrating = false;
  for (int i = 0; i < 3; i++) {
    if (calibMax[i] - calibMin[i] > CALIBRATION_MAX_RANGE_DPS) {
      Serial.println("[imu] calibration rejected: robot was moving, keeping previous bias");
      return;
    }
  }
  biasX = calibSum[0] / calibCount;
  biasY = calibSum[1] / calibCount;
  biasZ = calibSum[2] / calibCount;
  data.calibrated = true;
}

void imuUpdate(unsigned long now) {
  if (!data.ok) {
    if (now - lastRetryMs >= RETRY_INTERVAL_MS) {
      lastRetryMs = now;
      data.ok = configureSensor();
    }
    return;
  }
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) return;
  lastSampleMs = now;

  uint8_t raw[14];
  if (!readRegs(REG_ACCEL_XOUT_H, raw, sizeof(raw))) {
    data.readsFailed++;
    if (++consecutiveFails >= MAX_CONSECUTIVE_FAILS) {
      data.ok = false;
      lastRetryMs = now;
    }
    return;
  }
  consecutiveFails = 0;
  data.readsOk++;

  int16_t ax = (raw[0] << 8) | raw[1];
  int16_t ay = (raw[2] << 8) | raw[3];
  int16_t az = (raw[4] << 8) | raw[5];
  // raw[6..7] = temperature, intentionally ignored
  if (ax == 0 && ay == 0 && az == 0) {  // corrupted read (gravity is never exactly 0)
    data.readsFailed++;
    return;
  }
  int16_t gx = (raw[8] << 8) | raw[9];
  int16_t gy = (raw[10] << 8) | raw[11];
  int16_t gz = (raw[12] << 8) | raw[13];

  float gxDps = gx / GYRO_LSB_PER_DPS;
  float gyDps = gy / GYRO_LSB_PER_DPS;
  float gzDps = gz / GYRO_LSB_PER_DPS;

  if (data.calibrating) {
    float v[3] = {gxDps, gyDps, gzDps};
    for (int i = 0; i < 3; i++) {
      calibSum[i] += v[i];
      calibMin[i] = min(calibMin[i], v[i]);
      calibMax[i] = max(calibMax[i], v[i]);
    }
    if (++calibCount >= CALIBRATION_SAMPLES) finishCalibration();
  }

  data.ax = ax / ACCEL_LSB_PER_G;
  data.ay = ay / ACCEL_LSB_PER_G;
  data.az = az / ACCEL_LSB_PER_G;
  data.gx = gxDps - biasX;
  data.gy = gyDps - biasY;
  data.gz = gzDps - biasZ;

  data.rollDeg = atan2f(data.ay, data.az) * 180.0f / PI;
  data.pitchDeg = atan2f(-data.ax, sqrtf(data.ay * data.ay + data.az * data.az)) * 180.0f / PI;

  float mag = sqrtf(data.ax * data.ax + data.ay * data.ay + data.az * data.az);
  bool candidate = fabsf(mag - 1.0f) > JOLT_THRESHOLD_G;
  if (candidate) {
    // Corrupted reads are frequent on this wiring: confirm with an immediate second
    // read, which must be over the threshold too and agree with the first.
    uint8_t again[6];
    if (readRegs(REG_ACCEL_XOUT_H, again, sizeof(again))) {
      float bx = (int16_t)((again[0] << 8) | again[1]) / ACCEL_LSB_PER_G;
      float by = (int16_t)((again[2] << 8) | again[3]) / ACCEL_LSB_PER_G;
      float bz = (int16_t)((again[4] << 8) | again[5]) / ACCEL_LSB_PER_G;
      float mag2 = sqrtf(bx * bx + by * by + bz * bz);
      candidate = fabsf(mag2 - 1.0f) > JOLT_THRESHOLD_G && fabsf(mag2 - mag) < 0.5f;
    } else {
      candidate = false;
    }
  }
  if (candidate) {
    if (++overThresholdCount >= JOLT_CONSECUTIVE_SAMPLES &&
        (long)(now - joltCooldownUntil) >= 0) {
      joltCooldownUntil = now + JOLT_COOLDOWN_MS;
      data.joltCount++;
      data.lastJoltMs = now;
    }
  } else {
    overThresholdCount = 0;
  }
}

ImuData imuGetData() {
  return data;
}

#else  // ---- MINI_ULTRON_ENABLE_IMU 0: no IMU wired, harmless stub ----

static ImuData data;

void imuInit() { Serial.println("[imu] disabled (MINI_ULTRON_ENABLE_IMU is 0)"); }
void imuUpdate(unsigned long) {}
ImuData imuGetData() { return data; }  // ok=false, joltCount=0: never fires
void imuStartCalibration() {}

#endif
