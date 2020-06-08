#ifndef _TLC1543_H_
#define _TLC1543_H_

typedef struct {
	int spi_channel;
	int spi_fd;
	int eoc_pin;
	int is_inited;
} TLC1543;

/**
	@return -1, if failed
*/
int tlc1543_init(TLC1543 *tlc1543, int spi_channel, int spi_speed, int eoc_pin);

int tlc1543_read(const TLC1543 *tlc1543, int channel);

int *tlc1543_read_all(const TLC1543 *tlc1543, int values[16]);

int tlc1543_close(TLC1543 *tlc1543);

#endif
