/**
 * @file sensors.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Опрос датчиков
 */

#include <math.h>
#include <stddef.h>
#include <wiringPiI2C.h>

#include <log.h>
#include <sharedmem.h>
#include <sensors.h>
#include <timerfd.h>

static shm_t sensors_shm;

#define FILTER_CNT (8U)

typedef struct {
	double values[FILTER_CNT];
	size_t index;
} filter_t;

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
	fd = wiringPiI2CSetupInterface("/dev/i2c-1", DEVICE_ID);
	if (fd == -1) {
		log_err("Cannot setup i2c");
		return 1;
	}
	wiringPiI2CWriteReg8(fd, REG_POWER_CTL, 0b00001000);

	int timerfd;
	timerfd = timerfd_init(50ULL * TIME_MS, 50ULL * TIME_MS);
	if (timerfd < 0) {
		return 1;
	}

	filter_t f_x = {0,};
	filter_t f_y = {0,};
	filter_t f_z = {0,};

	while (wait_cycle(timerfd)) {
		sensors_status_t s;

		double ax, ay, az;
		int X;
		int Y;
		int Z;

		X = wiringPiI2CReadReg16(fd, REG_DATA_X_LOW);
		Y = wiringPiI2CReadReg16(fd, REG_DATA_Y_LOW);
		Z = wiringPiI2CReadReg16(fd, REG_DATA_Z_LOW);

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

		shm_map_write(&sensors_shm, &s, sizeof(s));
	}

	return 0;
}
