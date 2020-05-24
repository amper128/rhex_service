/**
 * @file gps.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Работа с датчиками
 */

#pragma once

#define DEVICE_ID (0x53)

#define REG_POWER_CTL (0x2D)
#define REG_DATA_X_LOW (0x32)
#define REG_DATA_X_HIGH (0x33)
#define REG_DATA_Y_LOW (0x34)
#define REG_DATA_Y_HIGH (0x35)
#define REG_DATA_Z_LOW (0x36)
#define REG_DATA_Z_HIGH (0x37)

typedef struct {
	double angle_x;
	double angle_y;
	double angle_z;
} sensors_status_t;

int sensors_init(void);

int sensors_main(void);
