#pragma once
// Protocolo RF 433 MHz entre control y carrito.
// COPIAR ESTE ARCHIVO TAL CUAL al proyecto del carrito para que ambos hablen igual.
//
// Capa fisica: UART 4800 baud 8N1 con la linea INVERTIDA
//   (Serial.begin(RF_BAUD, SERIAL_8N1, rx, tx, true)). Asi, en reposo el TX queda
//   en LOW = sin portadora, y el DO del receptor se lee correctamente.
//
// Trama en el aire:
//   [0xAA x4]  preambulo (entrena el AGC del receptor)
//   [0x2D 0xD4] sincronia
//   [5 bytes codificados Manchester -> 10 bytes]: type, b1, b2, b3, crc8
//
// Ciclo de 100 ms (half-duplex por tiempo): el control transmite el comando (~33 ms de aire)
// y el carrito responde con la telemetria justo despues (~33 ms), antes del siguiente ciclo.
//
// Control -> carrito (TYPE_CMD) cada 100 ms:
//   b1 = acelerador int8 (-100 reversa ... +100 adelante)
//   b2 = giro int8 (-100 izquierda ... +100 derecha), el servo sigue este valor
//   b3 = flags (ver F_*); el color RGB va en los bits 4..7
//   El estado de faros/RGB se envia siempre completo: si se pierde un paquete, el
//   siguiente lo corrige.
//
// Carrito -> control (TYPE_TELEM), responder tras CADA comando valido (10 Hz):
//   b1 = velocidad en km/h x10 (0..255)
//   b2 = RPM de los motores / 4 (0..255 -> 0..1020 rpm)
//   b3 = bateria del carrito en % (0xFF = desconocida)
//
// FAILSAFE del carrito: si pasan >500 ms sin comando valido, detener motores.

#include <stdint.h>

namespace proto {

constexpr uint32_t RF_BAUD       = 4800;
constexpr uint8_t  PREAMBLE_BYTE = 0xAA;
constexpr uint8_t  PREAMBLE_LEN  = 4;
constexpr uint8_t  SYNC0         = 0x2D;
constexpr uint8_t  SYNC1         = 0xD4;
constexpr uint8_t  FRAME_LEN     = 5;                 // type + 3 datos + crc
constexpr uint8_t  WIRE_LEN      = PREAMBLE_LEN + 2 + 2 * FRAME_LEN;  // 16 bytes
constexpr uint8_t  CAR_TELEMETRY_HZ = 5;              // minimo esperado del carrito

constexpr uint8_t TYPE_CMD   = 0xC1;
constexpr uint8_t TYPE_TELEM = 0xE1;

constexpr uint8_t F_HEADLIGHTS = 0x01;
constexpr uint8_t F_RGB_ON     = 0x02;
constexpr uint8_t F_GYRO       = 0x04;
constexpr uint8_t F_BRAKE      = 0x08;                // freno motor (IN1=IN2=1) mientras se mantiene
constexpr uint8_t COLOR_SHIFT  = 4;                   // bits 4..7 = indice de color

struct Command {
  int8_t  throttle;
  int8_t  steer;
  uint8_t flags;
};

struct Telemetry {
  uint8_t speedX10;
  uint8_t rpmDiv4;
  uint8_t battPct;
};

// Tabla de colores de los LEDs RGB de las ruedas (indice -> nombre y RGB).
// El ultimo indice (RAINBOW) es el efecto arcoiris, lo anima el carrito.
struct RgbColor { const char* name; uint8_t r, g, b; };
constexpr uint8_t RGB_COUNT = 9;
constexpr RgbColor RGB_TABLE[RGB_COUNT] = {
  {"ROJO",     255,   0,   0},
  {"NARANJA",  255,  90,   0},
  {"AMARILLO", 255, 200,   0},
  {"VERDE",      0, 255,   0},
  {"CIAN",       0, 255, 255},
  {"AZUL",       0,   0, 255},
  {"VIOLETA",  150,   0, 255},
  {"BLANCO",   255, 255, 255},
  {"ARCOIRIS",   0,   0,   0},
};

inline uint8_t crc8(const uint8_t* d, uint8_t n) {
  uint8_t c = 0;
  while (n--) {
    c ^= *d++;
    for (uint8_t i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
  }
  return c;
}

// Manchester: bit 1 -> 10, bit 0 -> 01. Un byte ocupa 2 bytes en el aire.
inline uint16_t manchesterEncode(uint8_t b) {
  uint16_t v = 0;
  for (int i = 7; i >= 0; i--) v = (uint16_t)((v << 2) | (((b >> i) & 1) ? 0x2 : 0x1));
  return v;
}

// Devuelve false si algun par de bits no es valido (error de recepcion).
inline bool manchesterDecode(uint16_t v, uint8_t& out) {
  uint8_t b = 0;
  for (int i = 7; i >= 0; i--) {
    uint8_t p = (v >> (2 * i)) & 0x3;
    if (p == 0x2) b |= (1 << i);
    else if (p != 0x1) return false;
  }
  out = b;
  return true;
}

}  // namespace proto
