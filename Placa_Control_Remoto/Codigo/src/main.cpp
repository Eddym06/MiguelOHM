// Control remoto KAKATA RC433 - ESP32-S3
//
//  JOY0 (eje vertical)    -> acelerar / retroceder (proporcional)
//  JOY1 (eje horizontal)  -> giro; el servo del carrito sigue este valor
//  Boton JOY1             -> alterna MANUAL <-> GYRO (control por inclinacion)
//  BTN1                   -> faros del carrito on/off
//  BTN3                   -> corto: siguiente color RGB; 2 s: apaga/enciende los RGB
//  BTN4                   -> corto: siguiente pantalla; 2 s: apaga/enciende el OLED
//
// El enlace con el carrito es RF 433 MHz (ver include/protocol.h).

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Preferences.h>
#include "config.h"
#include "protocol.h"
#include "Button.h"
#include "Rf433.h"
#include "Tilt.h"
#include "Ui.h"

// ---------------------------------------------------------------- joysticks
struct Axis {
  uint8_t pin;
  bool invert;
  int center;
  float filt;
  Axis(uint8_t p, bool inv) : pin(p), invert(inv), center(2048), filt(2048) {}
};

static Axis joyThrottle(PIN_JOY0_Y, JOY0_Y_INVERT);
static Axis joySteer(PIN_JOY1_X, JOY1_X_INVERT);

static float sampleAxis(uint8_t pin) {
  uint32_t s = 0;
  for (int i = 0; i < 4; i++) s += analogRead(pin);
  return s / 4.0f;
}

static void calibrate(Axis& a) {
  float s = 0;
  for (int i = 0; i < 20; i++) {
    s += sampleAxis(a.pin);
    delay(2);
  }
  a.center = (int)(s / 20);
  a.filt = a.center;
}

// Devuelve -100..100 con zona muerta y recorrido reescalado
static int8_t readAxis(Axis& a) {
  a.filt += (sampleAxis(a.pin) - a.filt) * 0.5f;
  float dev = a.filt - a.center;
  float span = (dev >= 0 ? (4095 - a.center) : a.center) * JOY_TRAVEL;
  if (span < 200) span = 200;
  float n = constrain(dev / span, -1.0f, 1.0f);
  float m = fabsf(n);
  if (m < JOY_DEADZONE) return 0;
  m = (m - JOY_DEADZONE) / (1.0f - JOY_DEADZONE);
  if (n < 0) m = -m;
  if (a.invert) m = -m;
  return (int8_t)lroundf(m * 100);
}

static int8_t axisFromAngle(float deg) {
  float m = fabsf(deg);
  if (m < TILT_DEADZONE_DEG) return 0;
  m = constrain((m - TILT_DEADZONE_DEG) / (TILT_FULL_DEG - TILT_DEADZONE_DEG), 0.0f, 1.0f);
  return (int8_t)lroundf((deg < 0 ? -m : m) * 100);
}

// ---------------------------------------------------------------- bateria
// Divisor R26/R27: Vadc = Vbat * 22k / 122k
static float readBatteryV() {
  uint32_t s = 0;
  for (int i = 0; i < 8; i++) s += analogReadMilliVolts(PIN_VBAT);
  float mv = s / 8.0f;
  return mv * (VBAT_R_TOP + VBAT_R_BOT) / VBAT_R_BOT / 1000.0f * VBAT_CAL;
}

// Curva aproximada de descarga de una celda Li-ion / LiPo de 1S
static uint8_t batteryPercent(float v) {
  static const float tv[] = {3.27f, 3.61f, 3.69f, 3.71f, 3.73f, 3.75f, 3.77f, 3.79f, 3.80f, 3.82f, 3.84f,
                             3.85f, 3.87f, 3.91f, 3.95f, 3.98f, 4.02f, 4.08f, 4.11f, 4.15f, 4.20f};
  static const uint8_t tp[] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
                               55, 60, 65, 70, 75, 80, 85, 90, 95, 100};
  const int n = sizeof(tv) / sizeof(tv[0]);
  if (v <= tv[0]) return 0;
  if (v >= tv[n - 1]) return 100;
  for (int i = 1; i < n; i++) {
    if (v <= tv[i]) {
      float f = (v - tv[i - 1]) / (tv[i] - tv[i - 1]);
      return (uint8_t)lroundf(tp[i - 1] + f * (tp[i] - tp[i - 1]));
    }
  }
  return 100;
}

// ---------------------------------------------------------------- estado global
static Ui ui;
static Rf433 rf;
static Tilt tilt;
static UiData data;

static Button btnMode(PIN_JOY1_BTN);
static Button btnLights(PIN_BTN1);
static Button btnRgb(PIN_BTN3, LONG_PRESS_MS);
static Button btnScreen(PIN_BTN4, LONG_PRESS_MS);
static Button btnBrake(PIN_JOY0_BTN);  // freno: activo mientras se mantiene presionado

static bool gyroMode = false;
static bool headlights = false;
static bool rgbOn = true;
static uint8_t rgbIdx = 0;
static int8_t throttle = 0, steer = 0;
static float battV = 0;

static uint32_t tInputs = 0, tCmd = 0, tUi = 0, tBatt = 0, tDbg = 0;

// ---------------------------------------------------------------- configuracion guardada
// Se conserva al apagar: faros, RGB (on/off y color), pantalla actual y OLED on/off.
// El modo GYRO NO se guarda: siempre se arranca en MANUAL por seguridad.
static Preferences prefs;
static bool prefsDirty = false;
static uint32_t prefsDirtyAt = 0;

static void markDirty(uint32_t now) {
  prefsDirty = true;
  prefsDirtyAt = now;
}

static void loadSettings() {
  prefs.begin("kakata", false);
  headlights = prefs.getBool("faros", false);
  rgbOn = prefs.getBool("rgb", true);
  rgbIdx = prefs.getUChar("color", 0);
  if (rgbIdx >= proto::RGB_COUNT) rgbIdx = 0;
  ui.setScreen(prefs.getUChar("pantalla", 0));
  if (!prefs.getBool("oled", true)) ui.setDisplayOn(false);
}

// Escribe 1.5 s despues del ultimo cambio, para no desgastar la flash
static void saveSettingsIfDue(uint32_t now) {
  if (!prefsDirty || now - prefsDirtyAt < 1500) return;
  prefsDirty = false;
  prefs.putBool("faros", headlights);
  prefs.putBool("rgb", rgbOn);
  prefs.putUChar("color", rgbIdx);
  prefs.putUChar("pantalla", ui.screen());
  prefs.putBool("oled", ui.isOn());
}

static void setLeds(uint32_t now) {
  digitalWrite(PIN_LED1, data.linked ? HIGH : (((now / 500) % 2) ? HIGH : LOW));
  digitalWrite(PIN_LED2, gyroMode ? HIGH : LOW);
  digitalWrite(PIN_LED3, headlights ? HIGH : LOW);
  digitalWrite(PIN_LED4, rgbOn ? HIGH : LOW);
  digitalWrite(PIN_LED6, (data.battPct <= 15 && ((now / 400) % 2)) ? HIGH : LOW);
}

static void handleButtons(uint32_t now) {
  btnBrake.update(now);

  // JOY1: MANUAL <-> GYRO
  if (btnMode.update(now) == Button::PRESS) {
    if (!gyroMode && !tilt.ok()) {
      ui.toast("SIN MPU6050", now);
    } else {
      gyroMode = !gyroMode;
      if (gyroMode) tilt.zero();  // la posicion actual de la mano es el neutro
      ui.toast(gyroMode ? "MODO GYRO" : "MODO MANUAL", now);
    }
  }

  // BTN1: faros
  if (btnLights.update(now) == Button::PRESS) {
    headlights = !headlights;
    ui.toast(headlights ? "FAROS ON" : "FAROS OFF", now);
    markDirty(now);
  }

  // BTN3: color RGB (corto) / apagar-encender RGB (2 s)
  Button::Event e = btnRgb.update(now);
  if (e == Button::SHORT_PRESS) {
    if (rgbOn) {
      rgbIdx = (rgbIdx + 1) % proto::RGB_COUNT;
      char buf[24];
      snprintf(buf, sizeof(buf), "COLOR %s", proto::RGB_TABLE[rgbIdx].name);
      ui.toast(buf, now);
      markDirty(now);
    } else {
      ui.toast("RGB APAGADO", now);
    }
  } else if (e == Button::LONG_PRESS) {
    rgbOn = !rgbOn;
    ui.toast(rgbOn ? "RGB ON" : "RGB OFF", now);
    markDirty(now);
  }

  // BTN4: pantalla (corto) / apagar-encender OLED (2 s)
  e = btnScreen.update(now);
  if (e == Button::SHORT_PRESS && ui.isOn()) {
    ui.nextScreen();
    markDirty(now);
  } else if (e == Button::LONG_PRESS) {
    ui.setDisplayOn(!ui.isOn());
    markDirty(now);
  }
}

static void readControls() {
  if (gyroMode) {
    float fwd = GYRO_SWAP_AXES ? tilt.roll() : tilt.pitch();
    float side = GYRO_SWAP_AXES ? tilt.pitch() : tilt.roll();
    if (GYRO_INVERT_FWD) fwd = -fwd;
    if (GYRO_INVERT_STEER) side = -side;
    throttle = axisFromAngle(fwd);
    steer = axisFromAngle(side);
  } else {
    throttle = readAxis(joyThrottle);
    steer = readAxis(joySteer);
  }
}

static void fillUi(uint32_t now) {
  const proto::Telemetry& t = rf.telemetry();
  data.linked = rf.linked(now);
  data.linkQuality = data.linked ? rf.quality() : 0;
  data.battPct = batteryPercent(battV);
  data.battV = battV;
  data.gyroMode = gyroMode;
  data.brake = btnBrake.isDown();
  data.throttle = throttle;
  data.steer = steer;
  data.speedValid = data.linked;
  data.speedKmh = t.speedX10 / 10.0f;
  data.rpm = t.rpmDiv4 * 4.0f;
  data.headlights = headlights;
  data.rgbOn = rgbOn;
  data.rgbIdx = rgbIdx;
  data.roll = tilt.roll();
  data.pitch = tilt.pitch();
  data.mpuOk = tilt.ok();
  data.txCount = rf.txCount();
  data.rxOk = rf.rxOk();
  data.rxBad = rf.rxBad();
  data.carBatt = t.battPct;
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);

  const uint8_t leds[] = {PIN_LED1, PIN_LED2, PIN_LED3, PIN_LED4, PIN_LED5, PIN_LED6};
  for (uint8_t p : leds) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }

  btnMode.begin();
  btnLights.begin();
  btnRgb.begin();
  btnScreen.begin();
  btnBrake.begin();

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);

  if (!ui.begin()) Serial.println("OLED no detectado");
  loadSettings();  // restaura la ultima configuracion
  ui.splash("Calibrando...");

  calibrate(joyThrottle);
  calibrate(joySteer);

  if (tilt.begin()) {
    tilt.update(millis());
    tilt.zero();
  } else {
    Serial.println("MPU6050 no detectado");
  }

  battV = readBatteryV();
  rf.begin();

  delay(700);  // deja ver el splash
  uint32_t now = millis();
  tInputs = tCmd = tUi = tBatt = tDbg = now;
}

void loop() {
  uint32_t now = millis();

  rf.update(now);
  handleButtons(now);
  saveSettingsIfDue(now);

  if (now - tInputs >= 20) {
    tInputs = now;
    tilt.update(now);
    readControls();
  }

  if (now - tBatt >= 250) {
    tBatt = now;
    float v = readBatteryV();
    battV += (v - battV) * 0.1f;
  }

  if (now - tCmd >= CMD_PERIOD_MS) {
    proto::Command c;
    c.throttle = throttle;
    c.steer = steer;
    c.flags = (headlights ? proto::F_HEADLIGHTS : 0) | (rgbOn ? proto::F_RGB_ON : 0) |
              (gyroMode ? proto::F_GYRO : 0) | (btnBrake.isDown() ? proto::F_BRAKE : 0) |
              (uint8_t)(rgbIdx << proto::COLOR_SHIFT);
    if (rf.sendCommand(c, now)) {
      tCmd = now;
      digitalWrite(PIN_LED5, !digitalRead(PIN_LED5));
    }
  }

  if (now - tUi >= UI_PERIOD_MS) {
    tUi = now;
    fillUi(now);
    setLeds(now);
    ui.draw(data, now);
  }

  if (now - tDbg >= 1000) {
    tDbg = now;
    Serial.printf("modo=%s thr=%d str=%d link=%d q=%u%% bat=%.2fV(%u%%) tx=%lu rx=%lu err=%lu\n",
                  gyroMode ? "GYRO" : "MANUAL", throttle, steer, data.linked, data.linkQuality, battV,
                  data.battPct, (unsigned long)rf.txCount(), (unsigned long)rf.rxOk(),
                  (unsigned long)rf.rxBad());
  }
}
