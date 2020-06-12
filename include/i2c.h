/**
 * @file i2c.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Работа с I2C
 */

#pragma once

#include <platform.h>

int i2c_open_dev(const char *device, int devId);

int i2c_write_reg_8(int fd, int reg, int value);

int i2c_read_reg_8(int fd, int reg);

int i2c_read_reg_16(int fd, int reg);
