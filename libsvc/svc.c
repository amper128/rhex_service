/**
 * @file svc.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Функции жизненного цикла микросервиса
 */

#include <stdio.h>
#include <sys/mman.h>
#include <time.h>

#include <log/log.h>
#include <svc/platform.h>
#include <svc/svc.h>
#include <svc/timerfd.h>

#define TIME_DEADLINE (1ULL * TIME_S)

static svc_context_t *svc_context;

const svc_context_t *
get_svc_context(void)
{
	return svc_context;
}

svc_context_t *
svc_create_context(const char svc_name[])
{
	svc_context_t *ctx = NULL;
	do {
		char file_name[256];
		snprintf(file_name, sizeof(file_name), "context_%s", svc_name);
		int fd = memfd_create(file_name, MFD_CLOEXEC);
		if (fd < 0) {
			log_err("context create \"%s\" error", svc_name);
			break;
		}

		if (ftruncate(fd, sizeof(svc_context_t)) == -1) {
			log_err("cannot ftruncate()");
			break;
		}

		void *map =
		    mmap(NULL, sizeof(svc_context_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		if (map == MAP_FAILED) {
			log_err("mmap() failed");
			break;
		}

		ctx = map;
	} while (false);

	return ctx;
}

uint64_t
svc_get_monotime(void)
{
	struct timespec ts;
	uint64_t result = 0ULL;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0) {
		result = (uint64_t)ts.tv_nsec + ((uint64_t)ts.tv_sec * TIME_S);
	}

	return result;
}

uint64_t
svc_get_time(void)
{
	struct timespec ts;
	uint64_t result = 0ULL;

	if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		result = (uint64_t)ts.tv_nsec + ((uint64_t)ts.tv_sec * TIME_S);
	}

	return result;
}

void
svc_init_context(svc_context_t *ctx)
{
	svc_context = ctx;
	ctx->watchdog = svc_get_monotime();
}

static inline bool
check_watchdog(const svc_context_t *ctx)
{
	bool result = true;

	uint64_t tm = svc_get_monotime();
	uint64_t diff = tm - ctx->watchdog;
	if (diff < INT64_MAX) {
		if (diff > TIME_DEADLINE) {
			log_warn("watchdog is out: %llu > %llu", diff, TIME_DEADLINE);
			result = false;
		}
	}

	return result;
}

bool
svc_cycle(void)
{
	bool result = true;

	const svc_context_t *ctx = get_svc_context();

	if (ctx->period > 0ULL) {
		if (!timerfd_wait(ctx->timerfd)) {
			result = false;
		}
	}

	if (!check_watchdog(ctx)) {
		result = false;
	}

	return result;
}
