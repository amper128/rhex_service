/**
 * @file main.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Точка входа сервиса, основные функции
 */

#include <gps.h>
#include <log.h>
#include <logger.h>
#include <motion.h>
#include <rhex_rc.h>
#include <sensors.h>
#include <telemetry.h>
#include <timerfd.h>

#include <sys/prctl.h>

#define SERVICES_MAX (32U)

typedef struct {
	pid_t pid;
	const char *name;
	log_buffer_t *log_buffer;
} svc_t;

static svc_t svc_list[SERVICES_MAX];
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
		/* we are new service */
		prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
		logger_init(name);
		exit(entry_point());
	}

	svc_list[svc_count].pid = pid;
	svc_list[svc_count].name = name;
	svc_list[svc_count].log_buffer = get_log_reader(name);

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
	} svc_list = {{{"gps", gps_init, gps_main},
		       {"sensors", sensors_init, sensors_main},
		       {"motion", motion_init, motion_main},
		       {"telemetry", telemetry_init, telemetry_main},
		       {"rc", rc_init, rc_main}},
		      5U};

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
	size_t i;
	for (i = 0U; i < svc_count; i++) {
		log_reader_print(svc_list[i].name, svc_list[i].log_buffer);
	}
}

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	logger_init("main");

	int timerfd;

	timerfd = timerfd_init(50ULL * TIME_MS, 50ULL * TIME_MS);
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
