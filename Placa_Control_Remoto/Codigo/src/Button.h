#pragma once
#include <Arduino.h>

// Boton activo en LOW con antirrebote por software.
// update() devuelve un evento por llamada:
//   PRESS       al presionar (para acciones inmediatas)
//   SHORT_PRESS al soltar si NO se llego a la pulsacion larga
//   LONG_PRESS  una sola vez al cumplir longMs con el boton aun presionado
class Button {
 public:
  enum Event : uint8_t { NONE, PRESS, SHORT_PRESS, LONG_PRESS };

  Button(uint8_t pin, uint32_t longMs = 2000) : pin_(pin), longMs_(longMs) {}

  void begin() { pinMode(pin_, INPUT_PULLUP); }

  Event update(uint32_t now) {
    bool raw = (digitalRead(pin_) == LOW);
    if (raw != lastRaw_) {
      lastRaw_ = raw;
      lastChange_ = now;
    }
    if ((now - lastChange_) >= DEBOUNCE_MS && raw != stable_) {
      stable_ = raw;
      if (stable_) {
        pressStart_ = now;
        longFired_ = false;
        return PRESS;
      }
      return longFired_ ? NONE : SHORT_PRESS;
    }
    if (stable_ && !longFired_ && (now - pressStart_) >= longMs_) {
      longFired_ = true;
      return LONG_PRESS;
    }
    return NONE;
  }

  bool isDown() const { return stable_; }

 private:
  static constexpr uint32_t DEBOUNCE_MS = 25;
  uint8_t pin_;
  uint32_t longMs_;
  bool lastRaw_ = false, stable_ = false, longFired_ = false;
  uint32_t lastChange_ = 0, pressStart_ = 0;
};
