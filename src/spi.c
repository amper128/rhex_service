/**
 * @file spi.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Работа с SPI
 */

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include <log.h>
#include <spi.h>

bool
spi_open_dev(spi_desc_t *spi_desc, int dev, int speed, int mode)
{
	bool result = false;

	spi_desc->spi_fd = -1;
	do {
		char spiDev[32];

		mode &= 3; // Mode is 0, 1, 2 or 3

		snprintf(spiDev, 31, "/dev/spidev0.%d", dev);

		if ((spi_desc->spi_fd = open(spiDev, O_RDWR)) < 0) {
			log_err("Unable to open SPI device: %s", strerror(errno));
			break;
		}

		/* Setup SPI params */

		if (ioctl(spi_desc->spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
			log_err("SPI Mode Change failure: %s", strerror(errno));
			close(spi_desc->spi_fd);
			break;
		}

		spi_desc->spi_bpw = 8U;
		if (ioctl(spi_desc->spi_fd, SPI_IOC_WR_BITS_PER_WORD, &spi_desc->spi_bpw) < 0) {
			log_err("SPI BPW Change failure: %s\n", strerror(errno));
			break;
		}

		spi_desc->spi_speed = speed;
		if (ioctl(spi_desc->spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_desc->spi_speed) < 0) {
			log_err("SPI Speed Change failure: %s", strerror(errno));
			close(spi_desc->spi_fd);
			break;
		}

		result = true;
	} while (false);

	if (!result) {
		if (spi_desc->spi_fd != -1) {
			close(spi_desc->spi_fd);
		}
	}

	return result;
}

int
spi_transfer(const spi_desc_t *spi_desc, uint8_t *data, int len)
{
	struct spi_ioc_transfer t;

	memset(&t, 0, sizeof(t));

	t.tx_buf = (unsigned long)data;
	t.rx_buf = (unsigned long)data;
	t.len = len;
	t.delay_usecs = spi_desc->spi_delay;
	t.speed_hz = spi_desc->spi_speed;
	t.bits_per_word = spi_desc->spi_bpw;

	return ioctl(spi_desc->spi_fd, SPI_IOC_MESSAGE(1), &t);
}
