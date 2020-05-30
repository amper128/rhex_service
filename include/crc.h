/**
 * @file crc.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции подсчета CRC
 */

#pragma once

#include <platform.h>

uint16_t vt_crc16(uint8_t data[], size_t size, uint16_t seed);
