#include "Rf433.h"
#include "config.h"

using namespace proto;

void Rf433::begin() {
  pinMode(PIN_RF_EN, OUTPUT);
  digitalWrite(PIN_RF_EN, RF_EN_ACTIVE_HIGH ? HIGH : LOW);

  RF_SERIAL.setRxBufferSize(256);
  RF_SERIAL.setTxBufferSize(128);  // evita que write() bloquee ~33 ms
  // Linea invertida: reposo = LOW = sin portadora (ver protocol.h)
  RF_SERIAL.begin(RF_BAUD, SERIAL_8N1, PIN_RF_RX, PIN_RF_TX, true);
}

bool Rf433::sendCommand(const Command& c, uint32_t now) {
  if (now < txBusyUntil_) return false;
  if (state_ == PAYLOAD) return false;  // el carrito esta respondiendo: no pisarlo

  uint8_t raw[FRAME_LEN] = {TYPE_CMD, (uint8_t)c.throttle, (uint8_t)c.steer, c.flags, 0};
  raw[FRAME_LEN - 1] = crc8(raw, FRAME_LEN - 1);

  uint8_t out[WIRE_LEN];
  uint8_t k = 0;
  for (uint8_t i = 0; i < PREAMBLE_LEN; i++) out[k++] = PREAMBLE_BYTE;
  out[k++] = SYNC0;
  out[k++] = SYNC1;
  for (uint8_t i = 0; i < FRAME_LEN; i++) {
    uint16_t m = manchesterEncode(raw[i]);
    out[k++] = m >> 8;
    out[k++] = m & 0xFF;
  }

  RF_SERIAL.write(out, WIRE_LEN);
  // 10 bits por byte; margen extra para que el receptor propio no lea el eco
  txBusyUntil_ = now + (uint32_t)WIRE_LEN * 10 * 1000 / RF_BAUD + 3;
  txCount_++;
  return true;
}

void Rf433::decodeFrame() {
  uint8_t raw[FRAME_LEN];
  for (uint8_t i = 0; i < FRAME_LEN; i++) {
    uint16_t v = ((uint16_t)buf_[2 * i] << 8) | buf_[2 * i + 1];
    if (!manchesterDecode(v, raw[i])) {
      rxBad_++;
      return;
    }
  }
  if (crc8(raw, FRAME_LEN - 1) != raw[FRAME_LEN - 1] || raw[0] != TYPE_TELEM) {
    rxBad_++;
    return;
  }
  telem_.speedX10 = raw[1];
  telem_.rpmDiv4 = raw[2];
  telem_.battPct = raw[3];
  lastRx_ = millis();
  if (lastRx_ == 0) lastRx_ = 1;
  rxOk_++;
  winRx_++;
}

void Rf433::update(uint32_t now) {
  // Mientras se transmite, el receptor puede captar nuestra propia senal: descartar
  if (now < txBusyUntil_) {
    while (RF_SERIAL.available()) RF_SERIAL.read();
    state_ = SEARCH;
    prev_ = 0;
  } else {
    while (RF_SERIAL.available()) {
      uint8_t b = (uint8_t)RF_SERIAL.read();
      if (state_ == SEARCH) {
        if (prev_ == SYNC0 && b == SYNC1) {
          state_ = PAYLOAD;
          n_ = 0;
          payloadStart_ = now;
        }
        prev_ = b;
      } else {
        buf_[n_++] = b;
        if (n_ == 2 * FRAME_LEN) {
          decodeFrame();
          state_ = SEARCH;
          prev_ = 0;
        }
      }
    }
    if (state_ == PAYLOAD && (now - payloadStart_) > 100) state_ = SEARCH;
  }

  // Calidad del enlace: telemetria recibida en 2 s vs. la esperada
  if (now - winStart_ >= 2000) {
    uint32_t expected = 2 * CAR_TELEMETRY_HZ;
    uint32_t q = winRx_ * 100 / expected;
    quality_ = q > 100 ? 100 : (uint8_t)q;
    winRx_ = 0;
    winStart_ = now;
  }
}

bool Rf433::linked(uint32_t now) const {
  return lastRx_ != 0 && (now - lastRx_) < LINK_TIMEOUT_MS;
}
