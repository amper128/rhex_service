/**
 * @file spi.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Работа с SPI
 */

#pragma once

#include <svc/platform.h>

typedef struct {
	int spi_fd;
	uint32_t spi_speed;
	uint16_t spi_delay;
	uint8_t spi_bpw;
	uint8_t spi_cs_change;
} spi_desc_t;

bool spi_open_dev(spi_desc_t *spi_desc, int dev, int speed, int mode);

int spi_transfer(const spi_desc_t *spi_desc, uint8_t *data, int len);
