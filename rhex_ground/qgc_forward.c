/**
 * @file qgc_forward.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Передача телеметрии в QOpenHD
 */

#include <arpa/inet.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utime.h>

#include <log/log.h>
#include <svc/sharedmem.h>
#include <svc/svc.h>
#include <wfb/wfb_status.h>

#include <private/qgc_forward.h>

#define PORT (5154)

static shm_t rx_status_shm;
static shm_t rx_status_uplink_shm;
static shm_t rx_status_rc_shm;
static shm_t rx_status_sysair_shm;

static wifibroadcast_rx_status_t *rx_status;
static wifibroadcast_rx_status_t *rx_status_uplink;
static wifibroadcast_rx_status_t_rc *rx_status_rc;
static wifibroadcast_rx_status_t_sysair *rx_status_sysair;

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

int
rssi_qgc_init(void)
{
	return 0;
}

int
rssi_qgc_main(void)
{
	int result;

	log_inf("rssi_qgc_forward started");

	result = shm_map_open("shm_rx_status", &rx_status_shm);
	result = shm_map_open("shm_tx_status", &rx_status_uplink_shm);
	result = shm_map_open("shm_rx_status_rc", &rx_status_rc_shm);
	result = shm_map_open("shm_rx_status_sysair", &rx_status_sysair_shm);

	uint64_t prev_cpu_time = svc_get_time();
	uint64_t delta = 0;

	uint8_t vbat_cap = 0;
	uint8_t is_charging = 0;
	uint32_t vbat_gnd = 0;

	// FILE *fp;

	int j = 0;

	struct sockaddr_in si_other_rssi;
	int s_rssi;
	size_t slen_rssi = sizeof(si_other_rssi);
	si_other_rssi.sin_family = AF_INET;
	si_other_rssi.sin_port = htons(PORT);
	si_other_rssi.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(si_other_rssi.sin_zero, '\0', sizeof(si_other_rssi.sin_zero));

	wifibroadcast_rx_status_forward_t wbcdata;

	wbcdata.damaged_block_cnt = 0;
	wbcdata.lost_packet_cnt = 0;
	wbcdata.skipped_packet_cnt = 0;
	wbcdata.injection_fail_cnt = 0;
	wbcdata.received_packet_cnt = 0;
	wbcdata.kbitrate = 0;
	wbcdata.kbitrate_measured = 0;
	wbcdata.kbitrate_set = 0;
	wbcdata.lost_packet_cnt_telemetry_up = 0;
	wbcdata.lost_packet_cnt_telemetry_down = 0;
	wbcdata.lost_packet_cnt_msp_up = 0;
	wbcdata.lost_packet_cnt_msp_down = 0;
	wbcdata.lost_packet_cnt_rc = 0;
	wbcdata.current_signal_joystick_uplink = 0;
	wbcdata.current_signal_telemetry_uplink = 0;
	wbcdata.joystick_connected = 0;
	wbcdata.HomeLon = 0;
	wbcdata.HomeLat = 0;
	wbcdata.cpuload_gnd = 0;
	wbcdata.temp_gnd = 0;
	wbcdata.cpuload_air = 0;
	wbcdata.temp_air = 0;
	wbcdata.vbat_capacity = 0;
	wbcdata.is_charging = 0;
	wbcdata.vbat_gnd_mv = 0;
	wbcdata.wifi_adapter_cnt = 0;

	for (j = 0; j < 6; ++j) {
		wbcdata.adapter[j].current_signal_dbm = -127;
		wbcdata.adapter[j].received_packet_cnt = 0;
		wbcdata.adapter[j].type = 0;
	}

	if ((s_rssi = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		log_err("Could not create UDP socket!");
	}

	while (svc_cycle()) {
		/* читаем состояния */
		shm_map_read(&rx_status_shm, (void **)&rx_status);
		shm_map_read(&rx_status_uplink_shm, (void **)&rx_status_uplink);
		shm_map_read(&rx_status_rc_shm, (void **)&rx_status_rc);
		shm_map_read(&rx_status_sysair_shm, (void **)&rx_status_sysair);

		size_t number_cards = rx_status->wifi_adapter_cnt;

		/* заполняем данные */
		wbcdata.damaged_block_cnt = rx_status->damaged_block_cnt;
		wbcdata.lost_packet_cnt = rx_status->lost_packet_cnt;
		wbcdata.skipped_packet_cnt = rx_status_sysair->skipped_fec_cnt;
		wbcdata.injection_fail_cnt = rx_status_sysair->injection_fail_cnt;
		wbcdata.received_packet_cnt = rx_status->received_packet_cnt;
		wbcdata.kbitrate = rx_status->kbitrate;

		wbcdata.kbitrate_measured = rx_status_sysair->bitrate_measured_kbit;
		wbcdata.kbitrate_set = rx_status_sysair->bitrate_kbit;
		wbcdata.lost_packet_cnt_telemetry_up = 0;
		/*wbcdata.lost_packet_cnt_telemetry_down = t_tdown->lost_packet_cnt;*/
		wbcdata.lost_packet_cnt_msp_up = 0;
		wbcdata.lost_packet_cnt_msp_down = 0;
		wbcdata.lost_packet_cnt_rc = rx_status_rc->lost_packet_cnt;
		/* RC uplink signal level */
		int8_t dbm = -127;
		size_t i;
		for (i = 0U; i < number_cards; i++) {
			if (rx_status_rc->adapter[i].current_signal_dbm > dbm) {
				dbm = rx_status_rc->adapter[i].current_signal_dbm;
			}
		}
		wbcdata.current_signal_joystick_uplink = dbm;
		/*wbcdata.current_signal_telemetry_uplink =
		 * t_uplink->adapter[0].current_signal_dbm;*/

		wbcdata.joystick_connected = 0;

		delta = svc_get_time() - prev_cpu_time;

		int r;
		(void)r;

		if (delta > (1ULL * TIME_S)) {
			prev_cpu_time = svc_get_time();

			/*if (wbcdata.HomeLon == 0 && wbcdata.HomeLat == 0) {

				float lonlat[2];

				lonlat[0] = 0.0;
				lonlat[1] = 0.0;

				fptr = fopen("/dev/shm/homepos", "rb");
				if (fptr != NULL) {
					r = fread(&lonlat, sizeof(lonlat), 2, fptr);

					fclose(fptr);

					wbcdata.HomeLat = lonlat[1];
					wbcdata.HomeLon = lonlat[0];
				}
			}*/

			/*fp = fopen("/sys/class/power_supply/axp288_fuel_gauge/capacity", "r");
			r = fscanf(fp, "%hhu", &vbat_cap);
			fclose(fp);

			fp = fopen("/sys/class/power_supply/axp288_fuel_gauge/voltage_now", "r");
			r = fscanf(fp, "%u", &vbat_gnd);
			fclose(fp);
			vbat_gnd /= 1000;

			char bat_status[32];
			fp = fopen("/sys/class/power_supply/axp288_fuel_gauge/status", "r");
			r = fscanf(fp, "%s", bat_status);
			fclose(fp);
			if (bat_status[0] == 'C') {
				is_charging = 1;
			} else {
				is_charging = 0;
			}*/
		}

		wbcdata.cpuload_gnd = get_cpuload();
		wbcdata.temp_gnd = get_cputemp();
		wbcdata.cpuload_air = rx_status_sysair->cpuload;

		wbcdata.vbat_capacity = vbat_cap;
		wbcdata.is_charging = is_charging;
		wbcdata.vbat_gnd_mv = vbat_gnd;

		wbcdata.temp_air = rx_status_sysair->temp;
		wbcdata.wifi_adapter_cnt = rx_status->wifi_adapter_cnt;

		size_t c;
		for (c = 0; c < number_cards; c++) {
			if (rx_status->adapter[c].signal_good > 0) {
				wbcdata.adapter[c].current_signal_dbm =
				    rx_status->adapter[c].current_signal_dbm;
			} else {
				wbcdata.adapter[c].current_signal_dbm = -127;
			}
			wbcdata.adapter[c].received_packet_cnt =
			    rx_status->adapter[c].received_packet_cnt;
			wbcdata.adapter[c].type = rx_status->adapter[c].type;
			wbcdata.adapter[c].signal_good = rx_status->adapter[c].signal_good;
		}

		if (sendto(s_rssi, &wbcdata, sizeof(wifibroadcast_rx_status_forward_t), 0,
			   (struct sockaddr *)&si_other_rssi, slen_rssi) == -1) {
			log_err("Could not send RSSI data!");
		}
	}

	return result;
}
