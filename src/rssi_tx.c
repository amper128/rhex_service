/**
 * @file rssi_tx.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Телеметрия канала связи и оборудования
 */

#include <stdio.h>

#include <rssi_tx.h>
#include <sharedmem.h>
#include <svc_context.h>
#include <wfb/wfb_tx.h>

struct rssi_data_t {
	int8_t signal;
	uint32_t lostpackets;
	int8_t signal_rc;
	uint32_t lostpackets_rc;
	uint8_t cpuload;
	uint8_t temp;
	uint32_t injected_block_cnt;
	uint32_t skipped_fec_cnt;
	uint32_t injection_fail_cnt;
	uint64_t injection_time_block;
	uint16_t bitrate_kbit;
	uint16_t bitrate_measured_kbit;
	uint8_t cts;
	uint8_t undervolt;
} __attribute__((__packed__));

static wfb_tx_t wfb_rssi_tx;

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
send_rssi()
{
	struct rssi_data_t rssi_data;

	rssi_data.cpuload = get_cpuload();
	rssi_data.temp = get_cputemp();

	wfb_tx_send_raw(&wfb_rssi_tx, (uint8_t *)&rssi_data, sizeof(rssi_data));
	usleep(1500);
	wfb_tx_send_raw(&wfb_rssi_tx, (uint8_t *)&rssi_data, sizeof(rssi_data));
	usleep(2000);
	wfb_tx_send_raw(&wfb_rssi_tx, (uint8_t *)&rssi_data, sizeof(rssi_data));

	return 0;
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

		while (svc_cycle()) {
			send_rssi();
		}
	} while (false);

	return result;
}
