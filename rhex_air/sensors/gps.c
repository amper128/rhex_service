/**
 * @file gps.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Работа с GPS
 */

#include <fcntl.h>
#include <string.h>
#include <termios.h>

#include <log/log.h>
#include <svc/sharedmem.h>
#include <svc/svc.h>

#include <gps.h>
#include <minmea.h>

static gps_status_t tmp_gps_status;

typedef struct {
	unsigned int speed;
	unsigned int value;
} speed_map;

static shm_t gps_shm;

static const speed_map speeds[] = {
    {B0, 0},	     {B50, 50},	      {B75, 75},	 {B110, 110},	    {B134, 134},
    {B150, 150},     {B200, 200},     {B300, 300},	 {B600, 600},	    {B1200, 1200},
    {B1800, 1800},   {B2400, 2400},   {B4800, 4800},	 {B9600, 9600},	    {B19200, 19200},
    {B38400, 38400}, {B57600, 57600}, {B115200, 115200}, {B230400, 230400}, {B460800, 460800},
};

#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

enum { NUM_SPEEDS = COUNT_OF(speeds) };

/*static unsigned int
baud2int(speed_t speed)
{
	unsigned char i;

	for (i = 0; i < NUM_SPEEDS; i++) {
		if (speed == speeds[i].speed) {
			return speeds[i].value;
		}
	}

	return 0;
}*/

static speed_t
int2baud(unsigned int value)
{
	unsigned char i;

	for (i = 0; i < NUM_SPEEDS; i++) {
		if (value == speeds[i].value) {
			return speeds[i].speed;
		}
	}

	return B0;
}

static int
serial_open(const char *name, const int baud)
{
	int fd = -1;

	do {
		fd = open(name, O_RDWR);
	} while ((fd < 0) && (errno == EINTR));

	if (fd < 0) {
		log_err("could not open serial device %s: %s", name, strerror(errno));
		return fd;
	}

	// disable echo on serial lines
	if (isatty(fd)) {
		struct termios ios;
		speed_t speed;

		tcgetattr(fd, &ios);
		ios.c_lflag = 0;	 /* disable ECHO, ICANON, etc... */
		ios.c_oflag &= (~ONLCR); /* Stop \n -> \r\n translation on output */
		ios.c_iflag &=
		    (~(ICRNL | INLCR));		/* Stop \r -> \n & \n -> \r translation on input */
		ios.c_iflag |= (IGNCR | IXOFF); /* Ignore \r & XON/XOFF on input */

		speed = int2baud(baud);

		if (speed != B0) {
			cfsetispeed(&ios, speed);
			cfsetospeed(&ios, speed);
		}

		tcsetattr(fd, TCSANOW, &ios);
	}

	return fd;
}

/*static int serial_get_ok(int fd)
{
	char buf[64+1];
	int n;
	int ret;

	do {
		n = 0;
		do {
			ret = read(fd, &buf[n], 1);

			if (ret < 0 && errno == EINTR) {
				continue;
			}

			if (ret < 0) {
				log_err("Error reading from serial port: %d", errno);
				return 0;
			}

			n ++;

		} while ((n < 64) && (buf[n-1] != '\n'));

		// Remove the trailing spaces
		while ((n > 0) && (buf[n-1] <= ' ') && (buf[n-1] != 0)) {
			n--;
		}
		buf[n] = 0;

		// Remove the leading spaces
		n = 0;
		while (buf[n]<= ' ' && buf[n] != 0) {
			n++;
		}

	} while (buf[0] == 0); // Ignore blank lines

	return strcmp(&buf[n],"OK") == 0;
}*/

static int
open_gps(const char *device, int baud)
{
	int gps_fd = -1;

	gps_fd = serial_open(device, baud);
	if (gps_fd < 0) {
		log_err("could not open gps serial device %s: %s", device, strerror(errno));
		return -1;
	}

	return gps_fd;
}

static void
parse_line(const char *line)
{
	enum minmea_sentence_id id;

	// log_dbg("parse '%s'", line);

	id = minmea_sentence_id(line, false);

	switch (id) {
	case MINMEA_SENTENCE_GGA: {
		struct minmea_sentence_gga frame;
		if (minmea_parse_gga(&frame, line)) {
			// log_dbg("$xxGGA: fix quality: %d", frame.fix_quality);
			if (frame.fix_quality > 0) {
				tmp_gps_status.has_fix = true;

				tmp_gps_status.latitude = minmea_tocoord(&frame.latitude);
				tmp_gps_status.longitude = minmea_tocoord(&frame.longitude);
				tmp_gps_status.altitude = minmea_tofloat(&frame.altitude);

				tmp_gps_status.hdop = minmea_tofloat(&frame.hdop);

				tmp_gps_status.sats_use = (uint8_t)frame.satellites_tracked;
			} else {
				tmp_gps_status.has_fix = false;
			}
		} else {
			log_warn("$xxGGA sentence is not parsed");
		}

		break;
	}

	case MINMEA_SENTENCE_GSV: {
		struct minmea_sentence_gsv frame;
		if (minmea_parse_gsv(&frame, line)) {
			/*log_dbg("$xxGSV: message %d of %d", frame.msg_nr, frame.total_msgs);
			log_dbg("$xxGSV: sattelites in view: %d", frame.total_sats);
			for (int i = 0; i < 4; i++) {
				log_dbg("$xxGSV: sat nr %d, elevation: %d, azimuth: %d, snr: %d
			dbm",
					frame.sats[i].nr,
					frame.sats[i].elevation,
					frame.sats[i].azimuth,
					frame.sats[i].snr);
			}*/

			tmp_gps_status.sats_view = frame.total_sats;
		} else {
			log_warn("$xxGSV sentence is not parsed");
		}

		break;
	}

	case MINMEA_SENTENCE_RMC: {
		struct minmea_sentence_rmc frame;
		if (minmea_parse_rmc(&frame, line)) {
			/*log_dbg("$xxRMC floating point degree coordinates and speed: (%f,%f) %f",
				minmea_tocoord(&frame.latitude),
				minmea_tocoord(&frame.longitude),
				minmea_tofloat(&frame.speed));*/

			tmp_gps_status.latitude = minmea_tocoord(&frame.latitude);
			tmp_gps_status.longitude = minmea_tocoord(&frame.longitude);
			tmp_gps_status.speed = minmea_tofloat(&frame.speed);
			tmp_gps_status.course = minmea_tofloat(&frame.course);

			shm_map_write(&gps_shm, &tmp_gps_status, sizeof(tmp_gps_status));
		} else {
			log_warn("$xxRMC sentence is not parsed");
		}

		break;
	}

	default:
		// log_warn("unknown GP '%s'", line);
		break;
	}
}

int
gps_init(void)
{
	/* do nothing */

	return 0;
}

int
gps_main(void)
{
	fd_set readfs;
	struct timeval tv;
	char buffer[128];
	char line[256];
	size_t line_len = 0U;

	int gps_fd = open_gps("/dev/serial1", 115200);
	if (gps_fd < 0) {
		return 0;
	}

	shm_map_open("shm_gps", &gps_shm);

	FD_ZERO(&readfs);
	FD_SET(gps_fd, &readfs);

	while (svc_cycle()) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		select(gps_fd + 1, &readfs, NULL, NULL, &tv);
		if (FD_ISSET(gps_fd, &readfs)) {
			int r = read(gps_fd, buffer, sizeof(buffer));
			if (r < 0) {
				log_err("cannot read");
				break;
			}

			int i;

			for (i = 0; i < r; i++) {
				if ((buffer[i] == '\r') || (buffer[i] == '\n')) {
					if (line_len > 0) {
						line[line_len] = 0;
						parse_line(line);
					}

					line_len = 0;
					continue;
				}

				line[line_len] = buffer[i];
				line_len++;
			}
		} else {
			/*log_warn("no data in select...");*/
			/* waiting for new data ... */
		}
	}

	return 0;
}
