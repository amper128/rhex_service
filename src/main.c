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
#include <svc_context.h>
#include <telemetry.h>
#include <timerfd.h>

#include <sys/prctl.h>

#define SERVICES_MAX (32U)

typedef struct {
	pid_t pid;
	const char *name;
	svc_context_t *ctx;
} svc_t;

static svc_t svc_list[SERVICES_MAX];
static size_t svc_count = 0U;
static svc_context_t *svc_main;

static int
start_svc(const char name[], int (*entry_point)(void))
{
	if (svc_count == SERVICES_MAX) {
		log_err("Service list overflow!");
		return -1;
	}

	log_inf("Starting svc \"%s\"...", name);

	svc_t *svc = &svc_list[svc_count];

	svc->ctx = svc_create_context(name);
	svc->ctx->log_buffer = logger_create(name);

	pid_t pid;

	pid = fork();
	if (pid == -1) {
		log_err("cannot fork");
		return -1;
	}

	if (pid == 0) {
		/* we are new service */
		svc_init_context(svc->ctx);
		prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
		logger_init();
		exit(entry_point());
	}

	svc->pid = pid;
	svc->name = name;

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
	log_reader_print("main", svc_main->log_buffer);
	size_t i;
	for (i = 0U; i < svc_count; i++) {
		svc_list[i].ctx->watchdog = svc_get_time();
		log_reader_print(svc_list[i].name, svc_list[i].ctx->log_buffer);
	}
}

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	svc_main = svc_create_context("main");
	svc_init_context(svc_main);
	svc_main->log_buffer = logger_create("main");
	logger_init();

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
