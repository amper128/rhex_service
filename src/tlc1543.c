#include <tlc1543.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>

#define _INIT_MAGIC_ 0x145200

static int _tlc1543_rw(const TLC1543 *tlc1543, int channel, unsigned char buf[2]);
static int _tlc1543_tranlate(unsigned char *buf);

/**
	@return -1, if failed
*/
int
tlc1543_init(TLC1543 *tlc1543, int spi_channel, int spi_speed, int eoc_pin)
{
	assert(tlc1543);
	wiringPiSetup();
	pinMode(eoc_pin, INPUT);
	tlc1543->spi_channel = spi_channel;
	tlc1543->eoc_pin = eoc_pin;
	tlc1543->spi_fd = wiringPiSPISetup(spi_channel, spi_speed);
	if (tlc1543->spi_fd > -1) {
		tlc1543->is_inited = _INIT_MAGIC_;
	}
	return tlc1543->spi_fd;
}

int
tlc1543_read(const TLC1543 *tlc1543, int channel)
{
	assert(tlc1543 && tlc1543->is_inited == _INIT_MAGIC_);
	int ret = -1;
	unsigned char buf[2];

	if (_tlc1543_rw(tlc1543, channel, buf) != -1) {
		if (_tlc1543_rw(tlc1543, channel, buf) != -1) {
			ret = _tlc1543_tranlate(buf);
		}
	}
	return ret;
}

int *
tlc1543_read_all(const TLC1543 *tlc1543, int values[11])
{
	assert(tlc1543 && tlc1543->is_inited == _INIT_MAGIC_);
	unsigned char buf[2] = {0, 0};
	if (_tlc1543_rw(tlc1543, 0, buf) != -1) {
		int i;
		for (i = 1; i < 15; i++) {
			if (_tlc1543_rw(tlc1543, i % 15, buf) != -1) {
				values[i - 1] = _tlc1543_tranlate(buf);
			}
		}
	}
	return values;
}

int
tlc1543_close(TLC1543 *tlc1543)
{
	assert(tlc1543 && tlc1543->is_inited == _INIT_MAGIC_);
	close(tlc1543->spi_fd);
	tlc1543->is_inited = 0;
	tlc1543->spi_fd = -1;
}

static int
_tlc1543_rw(const TLC1543 *tlc1543, int channel, unsigned char buf[2])
{
	while (digitalRead(tlc1543->eoc_pin) == 0) {
		/* do nothing */
	}
	buf[0] = ((unsigned char)channel) << 4;
	buf[1] = 0x00;
	return wiringPiSPIDataRW(tlc1543->spi_channel, buf, 2);
}

static int
_tlc1543_tranlate(unsigned char *buf)
{
	int value[3];
	value[0] = (buf[0] & 0xC0) >> 6;
	value[1] = (buf[0] & 0x3C) >> 2;
	value[2] = ((buf[0] & 0x03) << 2) | ((buf[1] & 0xC0) >> 6);
	return value[0] << 8 | value[1] << 4 | value[2];
}
