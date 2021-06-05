/**
 * @file i2c.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Работа с I2C
 */

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include <i2c.h>
#include <log.h>

static inline int
i2c_smbus_access(int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = rw;
	args.command = command;
	args.size = size;
	args.data = data;

	return ioctl(fd, I2C_SMBUS, &args);
}

int
i2c_open_dev(const char *device, int devId)
{
	int fd;

	if ((fd = open(device, O_RDWR)) < 0) {
		log_err("Unable to open I2C device: %s", strerror(errno));
	}

	if (ioctl(fd, I2C_SLAVE, devId) < 0) {
		log_err("Unable to select I2C device: %s", strerror(errno));
	}

	return fd;
}

int
i2c_write_reg_8(int fd, int reg, int value)
{
	union i2c_smbus_data data;

	data.byte = value;
	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, reg, I2C_SMBUS_BYTE_DATA, &data);
}

int
i2c_write_reg_16(int fd, int reg, int value)
{
	union i2c_smbus_data data;

	data.byte = value;
	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, reg, I2C_SMBUS_WORD_DATA, &data);
}

int
i2c_read_reg_8(int fd, int reg)
{
	union i2c_smbus_data data;

	if (i2c_smbus_access(fd, I2C_SMBUS_READ, reg, I2C_SMBUS_BYTE_DATA, &data)) {
		return -1;
	} else {
		return data.word & 0xFFFF;
	}
}

int
i2c_read_reg_16(int fd, int reg)
{
	union i2c_smbus_data data;

	if (i2c_smbus_access(fd, I2C_SMBUS_READ, reg, I2C_SMBUS_WORD_DATA, &data)) {
		return -1;
	} else {
		return data.word & 0xFFFF;
	}
}
