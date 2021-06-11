/**
 * @file main.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Точка входа сервиса, основные функции
 */

#include <camera.h>
#include <gps.h>
#include <svc/log.h>
#include <svc/logger.h>
#include <motion.h>
#include <rhex_rc.h>
#include <rhex_telemetry.h>
#include <rssi_tx.h>
#include <sensors.h>
#include <svc/sharedmem.h>
#include <svc/svc.h>
#include <svc/timerfd.h>
#include <wfb/wfb_status.h>

#include <sys/prctl.h>

#define SERVICES_MAX (32U)

typedef struct {
	pid_t pid;
	const char *name;
	svc_context_t *ctx;
} svc_t;

typedef struct {
	const char *name;
	int (*init)(void);
	int (*main)(void);
	uint64_t period;
} svc_desc_t;

static svc_t svc_list[SERVICES_MAX];
static size_t svc_count = 0U;
static svc_context_t *svc_main;

static int
start_svc(const svc_desc_t *svc_desc)
{
	if (svc_count == SERVICES_MAX) {
		log_err("Service list overflow!");
		return -1;
	}

	log_inf("Starting svc \"%s\"...", svc_desc->name);

	svc_t *svc = &svc_list[svc_count];

	svc->ctx = svc_create_context(svc_desc->name);
	svc->ctx->log_buffer = logger_create(svc_desc->name);

	pid_t pid;

	pid = fork();
	if (pid == -1) {
		log_err("cannot fork");
		return -1;
	}

	if (pid == 0) {
		/* we are new service */
		svc_init_context(svc->ctx);

		prctl(PR_SET_NAME, (unsigned long)svc_desc->name, 0, 0, 0);
		logger_init();

		/* setup timer */
		svc->ctx->period = svc_desc->period;
		if (svc->ctx->period > 0ULL) {
			svc->ctx->timerfd = timerfd_init(svc_desc->period, svc_desc->period);
			if (svc->ctx->timerfd < 0) {
				log_err("cannot setup timer");
				exit(1);
			}
		}

		exit(svc_desc->main());
	}

	svc->pid = pid;
	svc->name = svc_desc->name;

	svc_count++;

	return 0;
}

static int
start_microservices(void)
{
	static const struct {
		svc_desc_t svc[SERVICES_MAX];
		size_t count;
	} svc_start_list = {
	    {{"gps", gps_init, gps_main, 0ULL},
	     {"sensors", sensors_init, sensors_main, 50ULL * TIME_MS},
	     {"motion", motion_init, motion_main, 10ULL * TIME_MS},
	     {"telemetry", rhex_telemetry_init, rhex_telemetry_main, 100ULL * TIME_MS},
	     {"rc", rc_init, rc_main, 30ULL * TIME_MS},
	     {"rssi", rssi_tx_init, rssi_tx_main, (1ULL * TIME_S) / 3ULL},
	     {"camera", camera_init, camera_main, 0ULL}},
	    7U};

	size_t i;

	for (i = 0U; i < svc_start_list.count; i++) {
		svc_start_list.svc[i].init();
	}

	for (i = 0U; i < svc_start_list.count; i++) {
		start_svc(&svc_start_list.svc[i]);
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

	/* FIXME */
	shm_map_init("shm_rx_status", sizeof(wifibroadcast_rx_status_t));
	shm_map_init("shm_tx_status", sizeof(wifibroadcast_tx_status_t));
	shm_map_init("shm_rx_status_rc", sizeof(wifibroadcast_rx_status_t_rc));

	if (start_microservices()) {
		return 1;
	}

	while (timerfd_wait(timerfd)) {
		main_cycle();
	}
}
