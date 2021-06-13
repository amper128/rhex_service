/**
 * @file timerfd.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции работы с таймером
 */

#include <sys/timerfd.h>

#include <log/log.h>
#include <svc/timerfd.h>

int
timerfd_init(uint64_t start_nsec, uint64_t period_nsec)
{
	int fd;

	do {
		fd = timerfd_create(CLOCK_REALTIME, 0);
		if (fd == -1) {
			log_err("timerfd_create error");
			break;
		}

		struct itimerspec t;

		t.it_value.tv_sec = (long int)(start_nsec / TIME_S);
		t.it_value.tv_nsec = (long int)(start_nsec % TIME_S);

		t.it_interval.tv_sec = (long int)(period_nsec / TIME_S);
		t.it_interval.tv_nsec = (long int)(period_nsec % TIME_S);

		if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &t, NULL) == -1) {
			log_err("timerfd_settime error");
			close(fd);
			fd = -1;
		}
	} while (0);

	return fd;
}

bool
timerfd_wait(int fd)
{
	int result = true;
	uint64_t exp;
	int s;

	s = read(fd, &exp, sizeof(uint64_t));

	if (s != sizeof(uint64_t)) {
		log_err("timerfd read");
		result = false;
	}

	return result;
}
