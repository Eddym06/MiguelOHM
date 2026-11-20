#pragma once
#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

// Inclinacion (roll/pitch) del control con filtro complementario acel + giroscopio.
class Tilt {
 public:
  bool begin() {
    ok_ = mpu_.begin();
    if (ok_) {
      mpu_.setAccelerometerRange(MPU6050_RANGE_8_G);
      mpu_.setGyroRange(MPU6050_RANGE_500_DEG);
      mpu_.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }
    return ok_;
  }

  bool ok() const { return ok_; }

  void update(uint32_t now) {
    if (!ok_) return;
    sensors_event_t a, g, t;
    mpu_.getEvent(&a, &g, &t);

    float accRoll = atan2f(a.acceleration.y, a.acceleration.z) * RAD2DEG;
    float accPitch = atan2f(-a.acceleration.x,
                            sqrtf(a.acceleration.y * a.acceleration.y +
                                  a.acceleration.z * a.acceleration.z)) * RAD2DEG;

    if (!init_) {
      roll_ = accRoll;
      pitch_ = accPitch;
      init_ = true;
    } else {
      float dt = (now - last_) / 1000.0f;
      if (dt > 0.2f) dt = 0.2f;
      roll_ = ALPHA * (roll_ + g.gyro.x * RAD2DEG * dt) + (1.0f - ALPHA) * accRoll;
      pitch_ = ALPHA * (pitch_ + g.gyro.y * RAD2DEG * dt) + (1.0f - ALPHA) * accPitch;
    }
    last_ = now;
  }

  // Fija la posicion actual como "neutro"
  void zero() {
    refRoll_ = roll_;
    refPitch_ = pitch_;
  }

  float roll() const { return roll_ - refRoll_; }
  float pitch() const { return pitch_ - refPitch_; }

 private:
  static constexpr float ALPHA = 0.96f;
  static constexpr float RAD2DEG = 57.29578f;
  Adafruit_MPU6050 mpu_;
  bool ok_ = false, init_ = false;
  float roll_ = 0, pitch_ = 0, refRoll_ = 0, refPitch_ = 0;
  uint32_t last_ = 0;
};
