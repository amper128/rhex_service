/**
 * @file ina226.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Работа с INA226
 */

#include <i2c.h>
#include <ina226.h>

#include <byteswap.h>

int
ina226_open(int dev_id)
{
	int fd;

	fd = i2c_open_dev("/dev/i2c-1", dev_id);

	if (!ina226_ping(fd)) {
		close(fd);
		fd = -1;
	}

	return fd;
}

int
ina226_reset(int fd)
{
	uint16_t reg = bswap_16(INA226_RESET);
	return i2c_write_reg_16(fd, INA226_REG_CONFIG, reg);
}

bool
ina226_ping(int fd)
{
	uint16_t id;

	id = i2c_read_reg_16(fd, INA226_REG_ID);

	return (bswap_16(id) == INA226_DEVICE_ID);
}

int
ina226_set_shunt(int fd, float shunt)
{
	double lsb = 8.0 / 32768;
	double cal = 0.00512 / (lsb * shunt);

	uint16_t reg = bswap_16((int16_t)cal);
	return i2c_write_reg_16(fd, INA226_REG_CALIBRATION, reg);
}

int
ina226_get_voltage(int fd)
{
	int v;

	v = bswap_16(i2c_read_reg_16(fd, INA226_REG_BUSVOLTAGE));

	v = v + (v >> 2);

	return v;
}

int
ina226_get_shunt_voltage(int fd)
{
	int v;

	v = bswap_16(i2c_read_reg_16(fd, INA226_REG_SHUNTVOLTAGE));

	return v;
}

int
ina226_get_current(int fd)
{
	int c;

	c = bswap_16(i2c_read_reg_16(fd, INA226_REG_CURRENT));

	return c / 8;
}

int
ina226_get_power(int fd)
{
	int p;

	p = bswap_16(i2c_read_reg_16(fd, INA226_REG_POWER));

	p = (p * 3) + (p >> 3);

	return p;
}
