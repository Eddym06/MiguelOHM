#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Datos que muestra el OLED (los llena main.cpp)
struct UiData {
  bool linked = false;
  uint8_t linkQuality = 0;     // 0..100
  uint8_t battPct = 0;
  float battV = 0;
  bool gyroMode = false;
  bool brake = false;          // freno motor activo
  int8_t throttle = 0;         // -100..100
  int8_t steer = 0;            // -100..100
  float speedKmh = 0;
  float rpm = 0;
  bool speedValid = false;     // velocidad y rpm validas (hay telemetria)
  bool headlights = false;
  bool rgbOn = true;
  uint8_t rgbIdx = 0;
  float roll = 0, pitch = 0;   // grados, relativos al neutro
  bool mpuOk = false;
  uint32_t txCount = 0, rxOk = 0, rxBad = 0;
  uint8_t carBatt = 0xFF;      // 0xFF = desconocida
};

class Ui {
 public:
  enum Screen : uint8_t { SCR_DASH, SCR_GAUGE, SCR_SPEEDRPM, SCR_CONTROL, SCR_TILT, SCR_RF, SCR_COUNT };

  bool begin();
  void splash(const char* line);
  void nextScreen() { screen_ = (Screen)((screen_ + 1) % SCR_COUNT); }
  uint8_t screen() const { return screen_; }
  void setScreen(uint8_t s) { if (s < SCR_COUNT) screen_ = (Screen)s; }
  void setDisplayOn(bool on);
  bool isOn() const { return on_; }
  void toast(const char* text, uint32_t now, uint32_t ms = 1300);
  void draw(const UiData& d, uint32_t now);

 private:
  void statusBar(const UiData& d, uint32_t now);
  void drawWifi(int cx, int cy, uint8_t bars, bool linked, uint32_t now);
  void drawBattery(int xRight, int y, const UiData& d, uint32_t now);
  void screenDash(const UiData& d);
  void screenGauge(const UiData& d);
  void screenSpeedRpm(const UiData& d);
  void miniGauge(int cx, int cy, int R, float frac, bool redzone);
  void screenControl(const UiData& d);
  void screenTilt(const UiData& d);
  void screenRf(const UiData& d);
  void chip(int x, int y, int w, const char* text, bool filled);
  void arc(int cx, int cy, int r, float a0, float a1);

  Adafruit_SSD1306 d_{128, 64, &Wire, -1};
  Screen screen_ = SCR_DASH;
  bool on_ = true;
  float dispSpeed_ = 0;
  float dispRpm_ = 0;
  char toastText_[24] = {0};
  uint32_t toastUntil_ = 0;
};
