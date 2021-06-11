/**
 * @file rssi_tx.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Телеметрия канала связи и оборудования, передача
 */

#include <stdio.h>
#include <string.h>

#include <proto/telemetry.h>
#include <rssi_tx.h>
#include <svc/sharedmem.h>
#include <svc/svc.h>
#include <wfb/wfb_status.h>
#include <wfb/wfb_tx.h>

typedef struct {
	wifibroadcast_rx_status_t *rx_status;
	wifibroadcast_rx_status_t_rc *rx_status_rc;
	wifibroadcast_tx_status_t *tx_status;
} telemetry_data_t;

static wfb_tx_t wfb_rssi_tx;

static shm_t rx_status_shm;
static shm_t rx_status_rc_shm;
static shm_t tx_status_shm;

int
rssi_tx_init(void)
{
	return 0;
}

static uint8_t
get_cpuload(void)
{
	uint8_t cpu_load = 0U;
	static long double a[4], b[4];

	FILE *fp;
	int r;

	fp = fopen("/proc/stat", "r");
	r = fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
	fclose(fp);

	if (r > 0) {
		cpu_load = (((b[0] + b[1] + b[2]) - (a[0] + a[1] + a[2])) /
			    ((b[0] + b[1] + b[2] + b[3]) - (a[0] + a[1] + a[2] + a[3]))) *
			   100U;
	}

	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
	a[3] = b[3];

	return cpu_load;
}

static uint8_t
get_cputemp(void)
{
	uint8_t cputemp = 0U;
	FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
	int temp = 0;
	int r = fscanf(fp, "%d", &temp);
	fclose(fp);
	if (r > 0) {
		cputemp = (uint8_t)(temp / 1000);
	}

	return cputemp;
}

static int
send_rssi(const telemetry_data_t *td)
{
	struct rssi_data_t rssi_data;

	memset(&rssi_data, 0U, sizeof(rssi_data));

	int best_dbm = -127;
	int best_dbm_rc = -127;
	uint32_t c = 0;
	uint32_t number_cards = td->rx_status->wifi_adapter_cnt;
	uint32_t number_cards_rc = td->rx_status_rc->wifi_adapter_cnt;

	bool no_signal = true;
	bool no_signal_rc = true;

	for (c = 0; c < number_cards; c++) {
		if (td->rx_status->adapter[c].signal_good == 1) {
			if (best_dbm < td->rx_status->adapter[c].current_signal_dbm) {
				best_dbm = td->rx_status->adapter[c].current_signal_dbm;
			}

			no_signal = false;
		}
	}

	for (c = 0; c < number_cards_rc; c++) {
		if (td->rx_status_rc->adapter[c].signal_good == 1) {
			if (best_dbm_rc < td->rx_status_rc->adapter[c].current_signal_dbm) {
				best_dbm_rc = td->rx_status_rc->adapter[c].current_signal_dbm;
			}

			no_signal_rc = false;
		}
	}

	if (no_signal) {
		rssi_data.signal = -127;
	} else {
		rssi_data.signal = best_dbm;
	}

	if (no_signal_rc) {
		rssi_data.signal_rc = -127;
	} else {
		rssi_data.signal_rc = best_dbm_rc;
	}

	rssi_data.lostpackets = td->rx_status->lost_packet_cnt;
	rssi_data.lostpackets_rc = td->rx_status_rc->lost_packet_cnt;
	rssi_data.injected_block_cnt = td->tx_status->injected_block_cnt;
	rssi_data.skipped_fec_cnt = td->tx_status->skipped_fec_cnt;
	rssi_data.injection_fail_cnt = td->tx_status->injection_fail_cnt;
	rssi_data.injection_time_block = td->tx_status->injection_time_block;

	rssi_data.cpuload = get_cpuload();
	rssi_data.temp = get_cputemp();

	/* sending three times */
	wfb_tx_send_raw(&wfb_rssi_tx, (uint8_t *)&rssi_data, sizeof(rssi_data));
	usleep(1500);
	wfb_tx_send_raw(&wfb_rssi_tx, (uint8_t *)&rssi_data, sizeof(rssi_data));
	usleep(2000);
	wfb_tx_send_raw(&wfb_rssi_tx, (uint8_t *)&rssi_data, sizeof(rssi_data));

	return 0;
}

static void
telemetry_read(telemetry_data_t *td)
{
	shm_map_read(&rx_status_shm, (void **)&td->rx_status);
	shm_map_read(&rx_status_rc_shm, (void **)&td->rx_status_rc);
	shm_map_read(&tx_status_shm, (void **)&td->tx_status);
}

int
rssi_tx_main(void)
{
	int result = 0;

	do {
		result = wfb_tx_init(&wfb_rssi_tx, 63, true);
		if (result != 0) {
			break;
		}

		if (shm_map_open("shm_rx_status", &rx_status_shm) < 0) {
			break;
		}
		if (shm_map_open("shm_rx_status_rc", &rx_status_rc_shm) < 0) {
			break;
		}
		if (shm_map_open("shm_tx_status", &tx_status_shm) < 0) {
			break;
		}

		telemetry_data_t td;

		while (svc_cycle()) {
			telemetry_read(&td);
			send_rssi(&td);
		}
	} while (false);

	return result;
}
