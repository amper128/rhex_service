/**
 * @file rssi_rx.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Телеметрия канала связи и оборудования, прием
 */

#include <stdio.h>
#include <string.h>

#include <log/log.h>
#include <proto/telemetry.h>
#include <svc/netlink.h>
#include <svc/sharedmem.h>
#include <svc/svc.h>
#include <wfb/wfb_rx.h>
#include <wfb/wfb_status.h>

int dbm[6];
int ant[6];
int db[6];
int dbm_noise[6];
int dbm_last[6];
int quality[6];

long long tsft[6];

long long dbm_ts_prev[6];
long long dbm_ts_now[6];

static wifibroadcast_rx_status_t rx_status;
static wifibroadcast_rx_status_t rx_status_uplink;
static wifibroadcast_rx_status_t_rc rx_status_rc;
static wifibroadcast_rx_status_t_sysair rx_status_sysair;

static shm_t rx_status_shm;
static shm_t rx_status_uplink_shm;
static shm_t rx_status_rc_shm;
static shm_t rx_status_sysair_shm;

int
rssi_rx_init(void)
{
	int result = 0;

	do {
		shm_map_init("shm_rx_status", sizeof(wifibroadcast_rx_status_t));
		shm_map_init("shm_tx_status", sizeof(wifibroadcast_tx_status_t));
		shm_map_init("shm_rx_status_rc", sizeof(wifibroadcast_rx_status_t_rc));
		shm_map_init("shm_rx_status_sysair", sizeof(wifibroadcast_rx_status_t_sysair));
	} while (false);

	return result;
}

long long
current_timestamp()
{
	struct timeval te;

	gettimeofday(&te, NULL);

	long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;

	return milliseconds;
}

uint8_t
process_packet(wfb_rx_packet_t *rx_data)
{
	struct rssi_data_t *payloaddata = (struct rssi_data_t *)rx_data->data;

	log_dbg("signal:%d", payloaddata->signal);
	log_dbg("lostpackets:%d\n", payloaddata->lostpackets);
	log_dbg("signal_rc:%d\n", payloaddata->signal_rc);
	log_dbg("lostpackets_rc:%d\n", payloaddata->lostpackets_rc);
	log_dbg("cpuload:%d\n", payloaddata->cpuload);
	log_dbg("temp:%d\n", payloaddata->temp);
	log_dbg("injected_blocl_cnt:%d\n", payloaddata->injected_block_cnt);

	log_dbg("bitrate_kbit:%d\n", payloaddata->bitrate_kbit);
	log_dbg("bitrate_measured_kbit:%d\n", payloaddata->bitrate_measured_kbit);

	log_dbg("cts:%d\n", payloaddata->cts);
	log_dbg("undervolt:%d\n", payloaddata->undervolt);

	rx_status_uplink.adapter[0].current_signal_dbm = payloaddata->signal;
	rx_status_uplink.lost_packet_cnt = payloaddata->lostpackets;
	rx_status_rc.adapter[0].current_signal_dbm = payloaddata->signal_rc;
	rx_status_rc.lost_packet_cnt = payloaddata->lostpackets_rc;

	rx_status_sysair.cpuload = payloaddata->cpuload;
	rx_status_sysair.temp = payloaddata->temp;

	rx_status_sysair.skipped_fec_cnt = payloaddata->skipped_fec_cnt;
	rx_status_sysair.injected_block_cnt = payloaddata->injected_block_cnt;
	rx_status_sysair.injection_fail_cnt = payloaddata->injection_fail_cnt;
	rx_status_sysair.injection_time_block = payloaddata->injection_time_block;

	rx_status_sysair.bitrate_kbit = payloaddata->bitrate_kbit;
	rx_status_sysair.bitrate_measured_kbit = payloaddata->bitrate_measured_kbit;

	rx_status_sysair.cts = payloaddata->cts;
	rx_status_sysair.undervolt = payloaddata->undervolt;

	/* write to shm */
	shm_map_write(&rx_status_uplink_shm, &rx_status_uplink, sizeof(rx_status_uplink));
	shm_map_write(&rx_status_rc_shm, &rx_status_rc, sizeof(rx_status_rc));
	shm_map_write(&rx_status_sysair_shm, &rx_status_sysair, sizeof(rx_status_sysair));

	// write(STDOUT_FILENO, pu8Payload, 18);

	return (0);
}

void
status_memory_init(wifibroadcast_rx_status_t *s)
{
	s->received_block_cnt = 0;
	s->damaged_block_cnt = 0;
	s->received_packet_cnt = 0;
	s->lost_packet_cnt = 0;
	s->tx_restart_cnt = 0;
	s->wifi_adapter_cnt = 0;
	s->kbitrate = 0;
	s->current_air_datarate_kbit = 0;

	size_t i;
	for (i = 0; i < NL_MAX_IFACES; i++) {
		s->adapter[i].received_packet_cnt = 0;
		s->adapter[i].wrong_crc_cnt = 0;
		s->adapter[i].current_signal_dbm = -126;

		/* Set to 2 to see if it didn't get set later */
		s->adapter[i].type = 2;
	}
}

void
status_memory_init_rc(wifibroadcast_rx_status_t_rc *s)
{
	s->received_block_cnt = 0;
	s->damaged_block_cnt = 0;
	s->received_packet_cnt = 0;
	s->lost_packet_cnt = 0;
	s->tx_restart_cnt = 0;
	s->wifi_adapter_cnt = 0;

	size_t i;
	for (i = 0; i < NL_MAX_IFACES; ++i) {
		s->adapter[i].received_packet_cnt = 0;
		s->adapter[i].wrong_crc_cnt = 0;
		s->adapter[i].current_signal_dbm = -126;
	}
}

void
status_memory_init_sysair(wifibroadcast_rx_status_t_sysair *s)
{
	s->cpuload = 0;
	s->temp = 0;
	s->skipped_fec_cnt = 0;
	s->injected_block_cnt = 0;
	s->injection_fail_cnt = 0;
	s->injection_time_block = 0;
	s->bitrate_kbit = 0;
	s->bitrate_measured_kbit = 0;
	s->cts = 0;
	s->undervolt = 0;
}

int
rssi_rx_main(void)
{
	log_inf("RSSI RX started");

	wfb_rx_t rssi_rx = {
	    0,
	};

	int result = 0;

	result = wfb_rx_init(&rssi_rx, 63);
	if (result != 0) {
		return result;
	}

	result = shm_map_open("shm_rx_status", &rx_status_shm);
	result = shm_map_open("shm_tx_status", &rx_status_uplink_shm);
	result = shm_map_open("shm_rx_status_rc", &rx_status_rc_shm);
	result = shm_map_open("shm_rx_status_sysair", &rx_status_sysair_shm);

	status_memory_init(&rx_status);
	status_memory_init(&rx_status_uplink);
	status_memory_init_rc(&rx_status_rc);
	status_memory_init_sysair(&rx_status_sysair);

	rx_status.wifi_adapter_cnt = rssi_rx.count;
	rx_status_uplink.wifi_adapter_cnt = rssi_rx.count;
	rx_status_rc.wifi_adapter_cnt = rssi_rx.count;

	size_t i;
	for (i = 0; i < NL_MAX_IFACES; i++) {
		rx_status.adapter[i].current_signal_dbm = -126;
		rx_status.adapter[i].signal_good = 1;
	}

	shm_map_write(&rx_status_shm, &rx_status, sizeof(rx_status));
	shm_map_write(&rx_status_uplink_shm, &rx_status_uplink, sizeof(rx_status_uplink));
	shm_map_write(&rx_status_rc_shm, &rx_status_rc, sizeof(rx_status_rc));
	shm_map_write(&rx_status_sysair_shm, &rx_status_sysair, sizeof(rx_status_sysair));

	while (svc_cycle()) {
		wfb_rx_packet_t rx_data = {
		    0,
		};

		if (wfb_rx_packet(&rssi_rx, &rx_data) > 0) {
			result = process_packet(&rx_data);
		}
	}

	return 0;
}
