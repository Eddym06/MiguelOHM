#pragma once
// Enlace SPI P4 (maestro) <-> S3 (esclavo). Tramas de tamano fijo a ~50 Hz, full-duplex:
//   P4 -> S3: estado del carrito (p4_to_s3_t)
//   S3 -> P4: ordenes de la web (s3_to_p4_t)
// Cada trama: struct + relleno de ceros + CRC16 en los ultimos 2 bytes.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_LINK_FRAME_SIZE  192
#define SPI_LINK_MAGIC       0xCA7E
#define SPI_LINK_PERIOD_MS   20
#define LIDAR_SECTORS        36          // 10 grados por sector
#define LIDAR_NO_DATA        0xFFFF

// s3_to_p4_t.flags
#define S3F_STA_CONNECTED    0x01        // un celular esta asociado al WiFi
#define S3F_WS_CLIENT        0x02        // hay una pagina abierta (WebSocket)
// p4_to_s3_t.flags
#define P4F_WIFI_ALLOWED     0x01        // el WiFi puede estar encendido (433 inactivo)

// p4_to_s3_t.link_mode
#define LINK_NONE            0
#define LINK_RF433           1
#define LINK_WEB             2

// p4_to_s3_t.sensor_ok
#define SENS_GPS_FIX         0x01
#define SENS_LIDAR           0x02
#define SENS_IMU             0x04
#define SENS_INA             0x08
#define SENS_COMPASS         0x10
#define SENS_GPS_UART        0x20        // llegan tramas NMEA (aunque sin fix)

// drive_flags (mismos bits que rc433)
#define DRV_HEADLIGHTS       0x01
#define DRV_RGB_ON           0x02
#define DRV_AUTO             0x04        // modo autonomo activo
#define DRV_BRAKE            0x08
#define DRV_COLOR_SHIFT      4

// nav_state
#define NAVS_IDLE            0
#define NAVS_RUNNING         1
#define NAVS_AVOID           2
#define NAVS_ARRIVED         3
#define NAVS_NO_GPS          4
#define NAVS_BLOCKED         5

// nav_cmd
#define NAVC_NONE            0
#define NAVC_GOTO            1
#define NAVC_CANCEL          2

// cfg_op
#define CFG_OP_NONE          0
#define CFG_OP_SET           1
#define CFG_OP_GET           2
#define CFG_OP_RESET         3

// test_id
#define TEST_NONE            0
#define TEST_MOTOR_L         1           // valor: PWM -100..100 (lazo abierto)
#define TEST_MOTOR_R         2
#define TEST_SERVO           3           // valor: angulo en grados
#define TEST_HEADLIGHTS      4           // valor: 0/1
#define TEST_RGB             5           // valor: indice de color
#define TEST_SPOILER         6           // barrido de la tira trasera
#define TEST_PID_RPM         7           // valor: rpm objetivo para ambos motores (con PID)
#define TEST_COMPASS_CAL     8           // valor 1 = empezar, 0 = terminar y guardar
#define TEST_GYRO_RECAL      9

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  link_mode;
    uint8_t  nav_state;
    uint8_t  sensor_ok;
    uint8_t  drive_flags;
    int8_t   throttle;          // aplicado (-100..100)
    int8_t   steer;
    uint8_t  batt_pct;
    uint8_t  sats;
    uint16_t vbat_mv;
    int16_t  current_ma;
    int16_t  rpm_l;
    int16_t  rpm_r;
    uint16_t speed_x100;        // km/h * 100 (de encoders)
    uint16_t heading_d10;       // grados * 10, 0..3599
    int16_t  pitch_d10;
    int16_t  roll_d10;
    int16_t  servo_d10;
    uint16_t hdop_x100;
    int32_t  lat_e7;
    int32_t  lon_e7;
    int32_t  wp_lat_e7;
    int32_t  wp_lon_e7;
    uint16_t wp_dist_dm;        // decimetros
    uint16_t wp_bearing_d10;
    uint16_t gps_speed_x100;
    uint16_t gps_course_d10;
    uint8_t  cfg_ack;           // eco de cfg_seq ya atendido
    uint8_t  cfg_key;
    float    cfg_val;
    uint8_t  test_id;           // prueba en curso (0 = ninguna)
    uint8_t  gps_fix;           // calidad GGA
    uint16_t lidar_cm[LIDAR_SECTORS];
} p4_to_s3_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  seq;
    uint8_t  flags;
    int8_t   throttle;
    int8_t   steer;
    uint8_t  drive_flags;
    uint8_t  auto_mode;         // 1 = quiere modo autonomo
    uint8_t  nav_cmd;
    uint8_t  nav_seq;
    int32_t  wp_lat_e7;
    int32_t  wp_lon_e7;
    uint8_t  test_id;
    uint8_t  test_seq;
    int16_t  test_value;
    uint16_t test_ms;
    uint8_t  cfg_op;
    uint8_t  cfg_seq;
    uint8_t  cfg_key;
    float    cfg_val;
} s3_to_p4_t;

_Static_assert(sizeof(p4_to_s3_t) <= SPI_LINK_FRAME_SIZE - 2, "p4_to_s3_t no cabe en la trama");
_Static_assert(sizeof(s3_to_p4_t) <= SPI_LINK_FRAME_SIZE - 2, "s3_to_p4_t no cabe en la trama");

uint16_t spi_link_crc16(const uint8_t *d, size_t n);

// Copia el struct al buffer (relleno con ceros) y agrega el CRC16 al final.
void spi_link_seal(uint8_t buf[SPI_LINK_FRAME_SIZE], const void *msg, size_t msg_size);
// Verifica CRC y magic; copia el struct a out. Devuelve false si la trama no es valida.
bool spi_link_open(const uint8_t buf[SPI_LINK_FRAME_SIZE], void *out, size_t msg_size);

#ifdef __cplusplus
}
#endif
