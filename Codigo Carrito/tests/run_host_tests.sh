#!/usr/bin/env bash
# Compila y ejecuta las pruebas de la logica pura en la PC (gcc/MinGW).
set -e
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/kakata_host_tests"
gcc -std=gnu11 -Wall -Wextra -O1 -o "$OUT" \
  tests/host_tests.c \
  shared/src/rc433_protocol.c shared/src/spi_link.c \
  p4/main/logic/pid.c p4/main/logic/drive_mix.c p4/main/logic/battery.c p4/main/logic/nmea.c \
  p4/main/logic/ld_parser.c p4/main/logic/heading.c p4/main/logic/nav.c p4/main/logic/spoiler_fx.c \
  -Ishared/include -Ip4/main/logic -lm
"$OUT"
