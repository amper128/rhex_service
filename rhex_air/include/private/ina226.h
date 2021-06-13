/**
 * @file ina226.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Работа с INA226
 */

#pragma once

#include <svc/platform.h>

#define INA226_DEVICE_ID (0x2260)
#define INA226_RESET (0x8000)
#define INA226_CALIBRATION_REF 40960

#define INA226_REG_CONFIG (0x00)
#define INA226_REG_SHUNTVOLTAGE (0x01)
#define INA226_REG_BUSVOLTAGE (0x02)
#define INA226_REG_POWER (0x03)
#define INA226_REG_CURRENT (0x04)
#define INA226_REG_CALIBRATION (0x05)
#define INA226_REG_MASKENABLE (0x06)
#define INA226_REG_ALERTLIMIT (0x07)
#define INA226_REG_ID (0xFF)

#define INA226_BIT_SOL (0x8000)
#define INA226_BIT_SUL (0x4000)
#define INA226_BIT_BOL (0x2000)
#define INA226_BIT_BUL (0x1000)
#define INA226_BIT_POL (0x0800)
#define INA226_BIT_CNVR (0x0400)
#define INA226_BIT_AFF (0x0010)
#define INA226_BIT_CVRF (0x0008)
#define INA226_BIT_OVF (0x0004)
#define INA226_BIT_APOL (0x0002)
#define INA226_BIT_LEN (0x0001)

int ina226_open(int dev_id);

int ina226_reset(int fd);

bool ina226_ping(int fd);

int ina226_set_shunt(int fd, float shunt);

int ina226_get_voltage(int fd);

int ina226_get_shunt_voltage(int fd);

int ina226_get_current(int fd);

int ina226_get_power(int fd);
