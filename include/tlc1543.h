/**
 * @file tlc1543.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Работа с АЦП TLC1543
 */

#pragma once

#include <spi.h>

typedef struct {
	int spi_channel;
	spi_desc_t spi_desc;
	int eoc_pin;
	int is_inited;
} TLC1543;

/**
	@return -1, if failed
*/
int tlc1543_init(TLC1543 *tlc1543, int spi_channel, int spi_speed, int eoc_pin);

int tlc1543_read(const TLC1543 *tlc1543, int channel);

int *tlc1543_read_all(const TLC1543 *tlc1543, int values[16]);

int tlc1543_close(TLC1543 *tlc1543);
