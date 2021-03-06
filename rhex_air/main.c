/**
 * @file main.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Точка входа сервиса, основные функции
 */

#include <linux/nl80211.h>
#include <sys/prctl.h>

#include <log/log.h>
#include <log/read.h>
#include <svc/sharedmem.h>
#include <svc/svc.h>
#include <svc/timerfd.h>
#include <wfb/wfb_status.h>

#include <private/camera.h>
#include <private/gps.h>
#include <private/motion.h>
#include <private/rhex_rc.h>
#include <private/rhex_telemetry.h>
#include <private/rssi_tx.h>
#include <private/sensors.h>

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
	svc->ctx->log_buffer = log_create(svc_desc->name);

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
		log_init();

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
	log_print("main", svc_main->log_buffer);
	size_t i;
	for (i = 0U; i < svc_count; i++) {
		svc_list[i].ctx->watchdog = svc_get_monotime();
		log_print(svc_list[i].name, svc_list[i].ctx->log_buffer);
	}
}

static void
setup_wfb(void)
{
	if_desc_t wlan_list[NL_MAX_IFACES];
	int wlan_count;

	wlan_count = nl_get_wlan_list(wlan_list);
	log_dbg("wlan: %i", wlan_count);
	int i;

	for (i = 0; i < wlan_count; i++) {
		log_dbg("down %s", wlan_list[i].ifname);
		if (nl_link_down(&wlan_list[i])) {
			break;
		}
		log_dbg("set monitor %s", wlan_list[i].ifname);
		if (nl_wlan_set_monitor(&wlan_list[i])) {
			break;
		}
		log_dbg("up %s", wlan_list[i].ifname);
		if (nl_link_up(&wlan_list[i])) {
			break;
		}

		log_dbg("set freq %s", wlan_list[i].ifname);
		if (nl_wlan_set_freq(&wlan_list[i], 5200, NL80211_CHAN_WIDTH_20_NOHT,
				     NL80211_CHAN_NO_HT)) {
			log_err("cannot change freq %s", wlan_list[i].ifname);
			break;
		}
	}
}

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	svc_main = svc_create_context("main");
	svc_init_context(svc_main);
	svc_main->log_buffer = log_create("main");
	log_init();

	int timerfd;

	timerfd = timerfd_init(50ULL * TIME_MS, 50ULL * TIME_MS);
	if (timerfd < 0) {
		return 1;
	}

	setup_wfb();

	if (start_microservices()) {
		return 1;
	}

	while (timerfd_wait(timerfd)) {
		main_cycle();
	}
}
