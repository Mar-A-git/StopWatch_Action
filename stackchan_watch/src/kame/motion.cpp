#include "motion.h"

#include <M5Unified.h>

#include "config.h"

namespace motion {
namespace {

bool     imuOk = false;
float    peak = 0;        // poll と poll の間の一番大きい加速度（g）
float    lastPeak = 0;
uint32_t lastThrustMs = 0;

}  // namespace

void begin() {
  imuOk = M5.Imu.isEnabled();
  Serial.printf("[IMU] %s\n", imuOk ? "OK" : "NOT FOUND");
}

void sample() {
  if (!imuOk) return;
  M5.Imu.update();
  float ax, ay, az;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;
  const float a = sqrtf(ax * ax + ay * ay + az * az);
  if (a > peak) peak = a;
}

bool poll(uint32_t now) {
  sample();
  const float p = peak;
  peak = 0;
  lastPeak = p;
  if (p >= THRUST_LOG_G) Serial.printf("[IMU] peak %.2f g\n", p);
  if (p < THRUST_ACCEL_G) return false;
  if (now - lastThrustMs < THRUST_COOLDOWN_MS) return false;
  lastThrustMs = now;
  Serial.printf("[IMU] THRUST %.2f g\n", p);
  return true;
}

float lastPeakG() { return lastPeak; }

}  // namespace motion
