/**
 * @file sensors.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Опрос датчиков
 */

#include <math.h>

#include <i2c.h>
#include <ina226.h>
#include <log.h>
#include <sensors.h>
#include <sharedmem.h>
#include <svc_context.h>

static shm_t sensors_shm;

#define FILTER_CNT (8U)

typedef struct {
	double values[FILTER_CNT];
	size_t index;
} filter_t;

typedef struct {
	double values[64U];
	size_t index;
} filter64_t;

static double
filter_value(double val, filter_t *filter)
{
	size_t slot = filter->index % FILTER_CNT;

	filter->values[slot] = val;

	filter->index++;

	size_t i;
	double sum = 0.0;
	for (i = 0U; i < FILTER_CNT; i++) {
		sum += filter->values[i];
	}

	return (sum / (double)FILTER_CNT);
}

static double
filter64_value(double val, filter64_t *filter)
{
	size_t slot = filter->index % 64U;

	filter->values[slot] = val;

	filter->index++;

	size_t i;
	double sum = 0.0;
	for (i = 0U; i < 64U; i++) {
		sum += filter->values[i];
	}

	return (sum / (double)64U);
}

int
sensors_init(void)
{
	/* do nothing */

	return 0;
}

int
sensors_main(void)
{
	shm_map_open("shm_sensors", &sensors_shm);

	int fd;
	fd = i2c_open_dev("/dev/i2c-1", DEVICE_ID);
	if (fd == -1) {
		log_err("Cannot setup i2c");
		return 1;
	}
	i2c_write_reg_8(fd, REG_POWER_CTL, 0b00001000);

	int ina_fd;

	ina_fd = ina226_open(0x40);

	if (ina_fd == -1) {
		log_err("cannot connect INA226!");
		ina_fd = ina226_open(0x40);
		if (ina_fd == -1) {
			log_err("cannot connect INA226!");
			return 1;
		}
	}

	ina226_set_shunt(ina_fd, 0.01);

	filter_t f_x = {
	    0,
	};
	filter_t f_y = {
	    0,
	};
	filter_t f_z = {
	    0,
	};

	filter64_t f_vbat = {
	    0,
	};
	filter_t f_curr = {
	    0,
	};

	double pwr = 0.0;

	while (svc_cycle()) {
		sensors_status_t s;

		double ax, ay, az;
		int X;
		int Y;
		int Z;

		double v;
		double c;

		v = (double)ina226_get_voltage(ina_fd);
		v = filter64_value(v, &f_vbat);

		c = (double)ina226_get_current(ina_fd);
		c = filter_value(c, &f_curr);

		pwr = pwr + (c / 3600.0 / 20.0);

		X = i2c_read_reg_16(fd, REG_DATA_X_LOW);
		Y = i2c_read_reg_16(fd, REG_DATA_Y_LOW);
		Z = i2c_read_reg_16(fd, REG_DATA_Z_LOW);

		X = -(~(int16_t)X + 1);
		Y = -(~(int16_t)Y + 1);
		Z = -(~(int16_t)Z + 1);

		ax = filter_value(X, &f_x);
		ay = filter_value(Y, &f_y);
		az = filter_value(Z, &f_z);

		double angleX, angleY, angleZ;

		angleX = atan(ax / (sqrt(ay * ay + az * az)));
		angleY = atan(ay / (sqrt(ax * ax + az * az)));
		angleZ = atan((sqrt(ax * ax + ay * ay)) / az);

		s.angle_x = angleX * 180.0 / M_PI;
		s.angle_y = angleY * 180.0 / M_PI;
		s.angle_z = angleZ * 180.0 / M_PI;
		s.vbat = v / 1000.0;
		s.curr = c / 1000.0;
		s.pwr = pwr;

		shm_map_write(&sensors_shm, &s, sizeof(s));
	}

	return 0;
}
