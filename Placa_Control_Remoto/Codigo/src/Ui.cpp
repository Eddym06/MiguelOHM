#include "Ui.h"
#include "config.h"
#include "protocol.h"
#include <math.h>

static const float DEG2RAD = 0.0174532925f;

bool Ui::begin() {
  bool ok = d_.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  d_.clearDisplay();
  d_.setTextColor(SSD1306_WHITE);
  d_.display();
  return ok;
}

void Ui::setDisplayOn(bool on) {
  on_ = on;
  d_.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void Ui::toast(const char* text, uint32_t now, uint32_t ms) {
  strncpy(toastText_, text, sizeof(toastText_) - 1);
  toastText_[sizeof(toastText_) - 1] = 0;
  toastUntil_ = now + ms;
}

void Ui::splash(const char* line) {
  d_.clearDisplay();
  d_.setTextSize(2);
  d_.setCursor(22, 8);
  d_.print("KAKATA");
  d_.setCursor(34, 28);
  d_.print("RC 433");
  d_.drawFastHLine(10, 48, 108, SSD1306_WHITE);
  d_.setTextSize(1);
  int w = strlen(line) * 6;
  d_.setCursor((128 - w) / 2, 54);
  d_.print(line);
  d_.display();
}

// ---------------------------------------------------------------- helpers
void Ui::arc(int cx, int cy, int r, float a0, float a1) {
  float step = 40.0f / r;  // ~0.7 px entre puntos
  for (float a = a0; a <= a1; a += step) {
    d_.drawPixel(cx + lroundf(r * cosf(a * DEG2RAD)), cy + lroundf(r * sinf(a * DEG2RAD)),
                 SSD1306_WHITE);
  }
}

void Ui::chip(int x, int y, int w, const char* text, bool filled) {
  if (filled) {
    d_.fillRoundRect(x, y, w, 10, 2, SSD1306_WHITE);
    d_.setTextColor(SSD1306_BLACK);
  } else {
    d_.drawRoundRect(x, y, w, 10, 2, SSD1306_WHITE);
    d_.setTextColor(SSD1306_WHITE);
  }
  int tw = strlen(text) * 6;
  d_.setCursor(x + (w - tw) / 2 + 1, y + 1);
  d_.print(text);
  d_.setTextColor(SSD1306_WHITE);
}

// ---------------------------------------------------------------- barra superior
// Icono wifi: 3 arcos + punto. Sin enlace: parpadea y lleva una barra diagonal.
void Ui::drawWifi(int cx, int cy, uint8_t bars, bool linked, uint32_t now) {
  bool visible = linked || ((now / 500) % 2 == 0);
  if (!visible) return;
  for (uint8_t i = 0; i < 3; i++) {
    if (linked && i >= bars) continue;
    arc(cx, cy, 4 + i * 3, 225, 315);
  }
  d_.fillCircle(cx, cy, 1, SSD1306_WHITE);
  if (!linked) {
    // contorno negro para que la barra se vea sobre los arcos
    d_.drawLine(cx - 9, 2, cx + 9, 15, SSD1306_BLACK);
    d_.drawLine(cx - 10, 2, cx + 8, 15, SSD1306_BLACK);
    d_.drawLine(cx - 9, 1, cx + 9, 14, SSD1306_WHITE);
  }
}

void Ui::drawBattery(int xRight, int y, const UiData& d, uint32_t now) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%u%%", d.battPct);
  int tw = strlen(buf) * 6;
  int tx = xRight - tw;
  bool low = d.battPct <= 15;
  bool show = !low || ((now / 400) % 2 == 0);
  d_.setTextSize(1);
  d_.setCursor(tx, y + 1);
  if (show) d_.print(buf);

  int bx = tx - 18;
  d_.drawRect(bx, y, 15, 9, SSD1306_WHITE);
  d_.fillRect(bx + 15, y + 2, 2, 5, SSD1306_WHITE);
  int fill = (d.battPct * 11 + 50) / 100;
  if (show && fill > 0) d_.fillRect(bx + 2, y + 2, fill, 5, SSD1306_WHITE);
}

void Ui::statusBar(const UiData& d, uint32_t now) {
  uint8_t bars = d.linkQuality > 66 ? 3 : (d.linkQuality > 33 ? 2 : 1);
  drawWifi(10, 14, bars, d.linked, now);

  d_.setTextSize(1);
  if (d.brake) {  // aviso de freno en lugar del modo, en video inverso
    d_.fillRoundRect(24, 2, 42, 12, 2, SSD1306_WHITE);
    d_.setTextColor(SSD1306_BLACK);
    d_.setCursor(29, 4);
    d_.print("FRENO");
    d_.setTextColor(SSD1306_WHITE);
  } else {
    d_.setCursor(26, 4);
    d_.print(d.gyroMode ? "GYRO" : "MANUAL");
  }

  drawBattery(127, 3, d, now);
  d_.drawFastHLine(0, 15, 128, SSD1306_WHITE);
}

// ---------------------------------------------------------------- pantallas
// 0: tablero principal
void Ui::screenDash(const UiData& d) {
  char buf[8];
  if (!d.speedValid) strcpy(buf, "--");
  else if (dispSpeed_ < 10) snprintf(buf, sizeof(buf), "%.1f", dispSpeed_);
  else snprintf(buf, sizeof(buf), "%.0f", dispSpeed_);
  d_.setTextSize(3);
  d_.setCursor(2, 20);
  d_.print(buf);
  d_.setTextSize(1);
  d_.setCursor(2, 45);
  d_.print("km/h");

  chip(2, 53, 32, "FARO", d.headlights);
  chip(38, 53, 28, "RGB", d.rgbOn);

  // volante: gira con el servo
  const int cx = 98, cy = 40, r = 17;
  d_.drawCircle(cx, cy, r, SSD1306_WHITE);
  d_.fillCircle(cx, cy, 3, SSD1306_WHITE);
  float th = d.steer / 100.0f * 90.0f;
  const float spokes[3] = {0, 90, 180};
  for (int i = 0; i < 3; i++) {
    float a = (spokes[i] + th) * DEG2RAD;
    d_.drawLine(cx, cy, cx + lroundf((r - 1) * cosf(a)), cy + lroundf((r - 1) * sinf(a)),
                SSD1306_WHITE);
  }
  float ta = (270 + th) * DEG2RAD;  // marca superior del volante
  d_.fillCircle(cx + lroundf(r * cosf(ta)), cy + lroundf(r * sinf(ta)), 2, SSD1306_WHITE);

  // barra de acelerador (arriba = adelante)
  d_.drawRect(120, 18, 6, 44, SSD1306_WHITE);
  int h = abs(d.throttle) * 21 / 100;
  if (d.throttle > 0) d_.fillRect(122, 40 - h, 2, h, SSD1306_WHITE);
  else if (d.throttle < 0) d_.fillRect(122, 41, 2, h, SSD1306_WHITE);
  d_.drawFastHLine(118, 40, 10, SSD1306_WHITE);
}

// 1: velocimetro tipo odometro
void Ui::screenGauge(const UiData& d) {
  const int cx = 64, cy = 61, R = 40;
  arc(cx, cy, R, 180, 360);
  arc(cx, cy, R - 1, 180, 360);
  for (int i = 0; i <= 10; i++) {
    float a = (180 + 18 * i) * DEG2RAD;
    int r0 = (i % 5 == 0) ? R - 9 : R - 5;
    d_.drawLine(cx + lroundf(r0 * cosf(a)), cy + lroundf(r0 * sinf(a)),
                cx + lroundf((R - 2) * cosf(a)), cy + lroundf((R - 2) * sinf(a)), SSD1306_WHITE);
  }
  d_.setTextSize(1);
  char buf[8];
  d_.setCursor(14, 53);
  d_.print("0");
  snprintf(buf, sizeof(buf), "%d", (int)SPEED_MAX_KMH);
  d_.setCursor(108, 53);
  d_.print(buf);

  float frac = constrain(dispSpeed_ / SPEED_MAX_KMH, 0.0f, 1.0f);
  float a = (180 + 180 * frac) * DEG2RAD;
  d_.drawLine(cx, cy, cx + lroundf((R - 7) * cosf(a)), cy + lroundf((R - 7) * sinf(a)),
              SSD1306_WHITE);
  d_.drawLine(cx, cy - 1, cx + lroundf((R - 7) * cosf(a)), cy - 1 + lroundf((R - 7) * sinf(a)),
              SSD1306_WHITE);
  d_.fillCircle(cx, cy, 3, SSD1306_WHITE);

  // lectura digital (esquina superior izquierda)
  if (!d.speedValid) strcpy(buf, "--");
  else if (dispSpeed_ < 10) snprintf(buf, sizeof(buf), "%.1f", dispSpeed_);
  else snprintf(buf, sizeof(buf), "%.0f", dispSpeed_);
  d_.setTextSize(2);
  d_.setCursor(0, 19);
  d_.print(buf);
  d_.setTextSize(1);
  d_.setCursor(0, 37);
  d_.print("km/h");
}

// 2: velocimetro + cuentarrevoluciones, lado a lado
void Ui::miniGauge(int cx, int cy, int R, float frac, bool redzone) {
  arc(cx, cy, R, 180, 360);
  if (redzone) {  // ultimo 20 % con doble arco
    arc(cx, cy, R - 1, 324, 360);
    arc(cx, cy, R - 2, 324, 360);
  }
  for (int i = 0; i <= 5; i++) {
    float a = (180 + 36 * i) * DEG2RAD;
    int r0 = R - 6;
    d_.drawLine(cx + lroundf(r0 * cosf(a)), cy + lroundf(r0 * sinf(a)),
                cx + lroundf((R - 1) * cosf(a)), cy + lroundf((R - 1) * sinf(a)), SSD1306_WHITE);
  }
  float a = (180 + 180 * constrain(frac, 0.0f, 1.0f)) * DEG2RAD;
  d_.drawLine(cx, cy, cx + lroundf((R - 4) * cosf(a)), cy + lroundf((R - 4) * sinf(a)), SSD1306_WHITE);
  d_.fillCircle(cx, cy, 2, SSD1306_WHITE);
}

void Ui::screenSpeedRpm(const UiData& d) {
  char buf[16];
  d_.setTextSize(1);

  if (!d.speedValid) strcpy(buf, "-- km/h");
  else if (dispSpeed_ < 10) snprintf(buf, sizeof(buf), "%.1f km/h", dispSpeed_);
  else snprintf(buf, sizeof(buf), "%.0f km/h", dispSpeed_);
  d_.setCursor(32 - strlen(buf) * 3, 19);
  d_.print(buf);

  if (!d.speedValid) strcpy(buf, "-- rpm");
  else snprintf(buf, sizeof(buf), "%d rpm", (int)lroundf(dispRpm_));
  d_.setCursor(96 - strlen(buf) * 3, 19);
  d_.print(buf);

  miniGauge(32, 53, 24, dispSpeed_ / SPEED_MAX_KMH, false);
  miniGauge(96, 53, 24, dispRpm_ / RPM_MAX, true);

  d_.setCursor(32 - 9, 57);
  d_.print("VEL");
  d_.setCursor(96 - 9, 57);
  d_.print("RPM");
  d_.drawFastVLine(64, 18, 46, SSD1306_WHITE);
}

// 3: posicion de los controles
void Ui::screenControl(const UiData& d) {
  d_.drawRect(3, 18, 44, 44, SSD1306_WHITE);
  d_.drawFastHLine(3, 40, 44, SSD1306_WHITE);
  d_.drawFastVLine(25, 18, 44, SSD1306_WHITE);
  int px = 25 + d.steer * 19 / 100;
  int py = 40 - d.throttle * 19 / 100;
  d_.fillCircle(px, py, 3, SSD1306_WHITE);

  char buf[16];
  d_.setTextSize(1);
  d_.setCursor(56, 20);
  d_.print("ACELERAR");
  snprintf(buf, sizeof(buf), "%+d%%", d.throttle);
  d_.setCursor(56, 30);
  d_.print(buf);
  d_.setCursor(56, 42);
  d_.print("GIRO");
  snprintf(buf, sizeof(buf), "%+d%%", d.steer);
  d_.setCursor(56, 52);
  d_.print(buf);
  d_.setCursor(104, 52);
  d_.print(d.gyroMode ? "GYR" : "JOY");
}

// 3: horizonte artificial (inclinacion del control)
void Ui::screenTilt(const UiData& d) {
  const int cx = 64, cy = 40, R = 23;
  d_.drawCircle(cx, cy, R, SSD1306_WHITE);
  if (d.mpuOk) {
    float rr = constrain(d.roll, -90.0f, 90.0f) * DEG2RAD;
    float off = constrain(d.pitch, -40.0f, 40.0f) * 0.5f;
    float ux = cosf(rr), uy = sinf(rr);
    float nx = -uy, ny = ux;
    for (int line = 0; line < 3; line++) {
      float o = off + line * 6;
      for (int t = -R; t <= R; t++) {
        if (line > 0 && (t & 1)) continue;  // suelo punteado
        int x = cx + lroundf(t * ux + o * nx);
        int y = cy + lroundf(t * uy + o * ny);
        int dx = x - cx, dy = y - cy;
        if (dx * dx + dy * dy < (R - 1) * (R - 1)) d_.drawPixel(x, y, SSD1306_WHITE);
      }
    }
  } else {
    d_.setCursor(46, 38);
    d_.print("SIN MPU");
  }
  // mira fija
  d_.drawFastHLine(cx - 10, cy, 6, SSD1306_WHITE);
  d_.drawFastHLine(cx + 5, cy, 6, SSD1306_WHITE);
  d_.fillCircle(cx, cy, 1, SSD1306_WHITE);

  char buf[12];
  d_.setTextSize(1);
  d_.setCursor(0, 22);
  d_.print("ROLL");
  snprintf(buf, sizeof(buf), "%+d", (int)lroundf(d.roll));
  d_.setCursor(0, 32);
  d_.print(buf);
  d_.setCursor(92, 22);
  d_.print("PITCH");
  snprintf(buf, sizeof(buf), "%+d", (int)lroundf(d.pitch));
  d_.setCursor(92, 32);
  d_.print(buf);
}

// 4: estado del enlace RF y bateria
void Ui::screenRf(const UiData& d) {
  char buf[24];
  d_.setTextSize(1);
  d_.setCursor(0, 19);
  snprintf(buf, sizeof(buf), "TX:%lu RX:%lu", (unsigned long)d.txCount, (unsigned long)d.rxOk);
  d_.print(buf);
  d_.setCursor(0, 29);
  snprintf(buf, sizeof(buf), "ERR:%lu", (unsigned long)d.rxBad);
  d_.print(buf);

  d_.setCursor(0, 39);
  d_.print("SENAL");
  d_.drawRect(38, 38, 62, 9, SSD1306_WHITE);
  d_.fillRect(40, 40, d.linkQuality * 58 / 100, 5, SSD1306_WHITE);
  snprintf(buf, sizeof(buf), "%u%%", d.linkQuality);
  d_.setCursor(104, 39);
  d_.print(buf);

  d_.setCursor(0, 49);
  snprintf(buf, sizeof(buf), "CTRL %.2fV", d.battV);
  d_.print(buf);
  d_.setCursor(0, 57);
  if (d.carBatt == 0xFF) snprintf(buf, sizeof(buf), "CARRITO --");
  else snprintf(buf, sizeof(buf), "CARRITO %u%%", d.carBatt);
  d_.print(buf);
}

// ---------------------------------------------------------------- dibujo general
void Ui::draw(const UiData& d, uint32_t now) {
  if (!on_) return;

  float target = d.speedValid ? d.speedKmh : 0;
  dispSpeed_ += (target - dispSpeed_) * 0.35f;
  float targetRpm = d.speedValid ? d.rpm : 0;
  dispRpm_ += (targetRpm - dispRpm_) * 0.35f;

  d_.clearDisplay();
  d_.setTextColor(SSD1306_WHITE);
  statusBar(d, now);
  d_.setTextSize(1);
  switch (screen_) {
    case SCR_DASH: screenDash(d); break;
    case SCR_GAUGE: screenGauge(d); break;
    case SCR_SPEEDRPM: screenSpeedRpm(d); break;
    case SCR_CONTROL: screenControl(d); break;
    case SCR_TILT: screenTilt(d); break;
    case SCR_RF: screenRf(d); break;
    default: break;
  }

  if (now < toastUntil_) {
    int w = strlen(toastText_) * 6 + 14;
    int x = (128 - w) / 2;
    d_.fillRoundRect(x, 43, w, 17, 4, SSD1306_BLACK);
    d_.drawRoundRect(x, 43, w, 17, 4, SSD1306_WHITE);
    d_.setTextSize(1);
    d_.setCursor(x + 7, 48);
    d_.print(toastText_);
  }
  d_.display();
}
