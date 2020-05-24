/**
 * @file main.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Точка входа сервиса, основные функции
 */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include <gps.h>
#include <log.h>
#include <motion.h>
#include <sensors.h>
#include <telemetry.h>
#include <rhex_rc.h>
#include <timerfd.h>

#define SERVICES_MAX (32U)

static pid_t svc_pids[SERVICES_MAX];
static size_t svc_count = 0U;

static int
start_svc(const char name[], int (*entry_point)(void))
{
	if (svc_count == SERVICES_MAX) {
		log_err("Service list overflow!");
		return -1;
	}

	pid_t pid;

	log_inf("Starting svc \"%s\"...", name);

	pid = fork();
	if (pid == -1) {
		log_err("cannot fork");
		return -1;
	}

	if (pid == 0) {
		exit(entry_point());
	}

	svc_pids[svc_count] = pid;
	svc_count++;

	return 0;
}

static int
start_microservices(void)
{
	static struct {
		struct {
			const char *name;
			int (*init)(void);
			int (*main)(void);
		} svc[SERVICES_MAX];
		size_t count;
	} svc_list = {
		{
			{"gps", 	gps_init,	gps_main},
			{"sensors",	sensors_init,	sensors_main},
			{"motion",	motion_init,	motion_main},
			{"telemetry",	telemetry_init,	telemetry_main},
			{"rc",		rc_init,	rc_main}
		},
		5U
	};

	size_t i;

	for (i = 0U; i < svc_list.count; i++) {
		svc_list.svc[i].init();
	}

	for (i = 0U; i < svc_list.count; i++) {
		start_svc(svc_list.svc[i].name, svc_list.svc[i].main);
	}

	return 0;
}

static void
main_cycle(void)
{
	/* do nothing */
}

int
main(int argc, char **argv)
{
	int timerfd;

	timerfd = timerfd_init(100ULL * TIME_MS, 100ULL * TIME_MS);
	if (timerfd < 0) {
		return 1;
	}

	if (start_microservices()) {
		return 1;
	}

	while (wait_cycle(timerfd)) {
		main_cycle();
	}
}
