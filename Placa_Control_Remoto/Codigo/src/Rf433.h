#pragma once
#include <Arduino.h>
#include "protocol.h"

// Enlace RF 433 MHz (transmisor + receptor ASK) sobre un UART.
class Rf433 {
 public:
  void begin();
  // Llamar seguido: lee y decodifica la telemetria del carrito.
  void update(uint32_t now);
  // Envia un comando. Devuelve false si todavia se esta transmitiendo el anterior.
  bool sendCommand(const proto::Command& c, uint32_t now);

  bool linked(uint32_t now) const;
  uint8_t quality() const { return quality_; }       // 0..100
  const proto::Telemetry& telemetry() const { return telem_; }
  uint32_t txCount() const { return txCount_; }
  uint32_t rxOk() const { return rxOk_; }
  uint32_t rxBad() const { return rxBad_; }

 private:
  enum State : uint8_t { SEARCH, PAYLOAD };
  void decodeFrame();

  State state_ = SEARCH;
  uint8_t prev_ = 0;
  uint8_t buf_[2 * proto::FRAME_LEN];
  uint8_t n_ = 0;
  uint32_t payloadStart_ = 0;

  proto::Telemetry telem_ = {0, 0, 0xFF};
  uint32_t lastRx_ = 0;
  uint32_t txBusyUntil_ = 0;
  uint32_t txCount_ = 0, rxOk_ = 0, rxBad_ = 0;

  uint32_t winStart_ = 0, winRx_ = 0;
  uint8_t quality_ = 0;
};
