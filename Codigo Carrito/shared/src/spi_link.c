#include "spi_link.h"
#include <string.h>

uint16_t spi_link_crc16(const uint8_t *d, size_t n)
{
    uint16_t crc = 0xFFFF;  // CRC-16/CCITT-FALSE
    while (n--) {
        crc ^= (uint16_t)(*d++) << 8;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void spi_link_seal(uint8_t buf[SPI_LINK_FRAME_SIZE], const void *msg, size_t msg_size)
{
    memset(buf, 0, SPI_LINK_FRAME_SIZE);
    memcpy(buf, msg, msg_size);
    uint16_t crc = spi_link_crc16(buf, SPI_LINK_FRAME_SIZE - 2);
    buf[SPI_LINK_FRAME_SIZE - 2] = (uint8_t)(crc >> 8);
    buf[SPI_LINK_FRAME_SIZE - 1] = (uint8_t)(crc & 0xFF);
}

bool spi_link_open(const uint8_t buf[SPI_LINK_FRAME_SIZE], void *out, size_t msg_size)
{
    uint16_t crc = spi_link_crc16(buf, SPI_LINK_FRAME_SIZE - 2);
    uint16_t got = (uint16_t)((buf[SPI_LINK_FRAME_SIZE - 2] << 8) | buf[SPI_LINK_FRAME_SIZE - 1]);
    if (crc != got) return false;
    uint16_t magic = (uint16_t)(buf[0] | (buf[1] << 8));
    if (magic != SPI_LINK_MAGIC) return false;
    memcpy(out, buf, msg_size);
    return true;
}
