/**
 * @file rssi_rx.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Телеметрия канала связи и оборудования, прием
 */

#include <proto/telemetry.h>
#include <svc/sharedmem.h>
#include <svc/svc.h>
#include <wfb/wfb_rx.h>
#include <wfb/wfb_status.h>

#include <private/rssi_rx.h>

static wifibroadcast_rx_status_t rx_status_uplink;
static wifibroadcast_rx_status_t_sysair rx_status_sysair;

static shm_t rx_status_uplink_shm;
static shm_t rx_status_sysair_shm;

int
rssi_rx_init(void)
{
	int result = 1;

	do {
		if (!shm_map_init("shm_tx_status", sizeof(wifibroadcast_tx_status_t))) {
			break;
		}

		if (!shm_map_init("shm_rx_status_sysair",
				  sizeof(wifibroadcast_rx_status_t_sysair))) {
			break;
		}

		result = 0;
	} while (false);

	return result;
}

static uint8_t
process_packet(wfb_rx_packet_t *rx_data)
{
	struct rssi_data_t *payloaddata = (struct rssi_data_t *)rx_data->data;

	/*log_dbg("signal: %d", payloaddata->signal);
	log_dbg("lostpackets: %d", payloaddata->lostpackets);
	log_dbg("signal_rc: %d", payloaddata->signal_rc);
	log_dbg("lostpackets_rc: %d", payloaddata->lostpackets_rc);
	log_dbg("cpuload: %d", payloaddata->cpuload);
	log_dbg("temp: %d", payloaddata->temp);
	log_dbg("injected_blocl_cnt: %d", payloaddata->injected_block_cnt);

	log_dbg("bitrate_kbit: %d", payloaddata->bitrate_kbit);
	log_dbg("bitrate_measured_kbit: %d", payloaddata->bitrate_measured_kbit);

	log_dbg("cts: %d", payloaddata->cts);
	log_dbg("undervolt: %d", payloaddata->undervolt);*/

	rx_status_uplink.adapter[0].current_signal_dbm = payloaddata->signal;
	rx_status_uplink.lost_packet_cnt = payloaddata->lostpackets;

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
	shm_map_write(&rx_status_sysair_shm, &rx_status_sysair, sizeof(rx_status_sysair));

	return (0);
}

static void
status_memory_init(wifibroadcast_rx_status_t *s)
{
	s->received_block_cnt = 0U;
	s->damaged_block_cnt = 0U;
	s->received_packet_cnt = 0U;
	s->lost_packet_cnt = 0U;
	s->tx_restart_cnt = 0U;
	s->wifi_adapter_cnt = 0U;
	s->kbitrate = 0U;
	s->current_air_datarate_kbit = 0U;

	size_t i;
	for (i = 0U; i < NL_MAX_IFACES; i++) {
		s->adapter[i].received_packet_cnt = 0U;
		s->adapter[i].wrong_crc_cnt = 0U;
		s->adapter[i].current_signal_dbm = -127;
		s->adapter[i].signal_good = 0;

		/* Set to 2 to see if it didn't get set later */
		s->adapter[i].type = 2;
	}
}

static void
status_memory_init_sysair(wifibroadcast_rx_status_t_sysair *s)
{
	s->cpuload = 0U;
	s->temp = 0U;
	s->skipped_fec_cnt = 0U;
	s->injected_block_cnt = 0U;
	s->injection_fail_cnt = 0U;
	s->injection_time_block = 0U;
	s->bitrate_kbit = 0U;
	s->bitrate_measured_kbit = 0U;
	s->cts = 0U;
	s->undervolt = 0U;
}

int
rssi_rx_main(void)
{
	log_inf("RSSI RX started");

	wfb_rx_t rssi_rx = {
	    0,
	};

	int result = 0;

	do {
		result = wfb_rx_init(&rssi_rx, 63);
		if (result != 0) {
			break;
		}

		if (!shm_map_open("shm_tx_status", &rx_status_uplink_shm)) {
			log_err("cannot open shm_tx_status");
			result = 1;
			break;
		}

		if (!shm_map_open("shm_rx_status_sysair", &rx_status_sysair_shm)) {
			log_err("cannot open shm_rx_status_sysair");
			result = 1;
			break;
		}

		status_memory_init(&rx_status_uplink);
		status_memory_init_sysair(&rx_status_sysair);

		rx_status_uplink.wifi_adapter_cnt = rssi_rx.count;

		shm_map_write(&rx_status_uplink_shm, &rx_status_uplink, sizeof(rx_status_uplink));
		shm_map_write(&rx_status_sysair_shm, &rx_status_sysair, sizeof(rx_status_sysair));

		while (svc_cycle()) {
			wfb_rx_packet_t rx_data = {
			    0,
			};

			if (wfb_rx_packet(&rssi_rx, &rx_data) > 0) {
				result = process_packet(&rx_data);
			}
		}
	} while (false);

	return result;
}
