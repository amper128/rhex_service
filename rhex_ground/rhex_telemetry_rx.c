
#include <fcntl.h> // serialport
#include <string.h>
#include <sys/resource.h>
#include <time.h>

#include <log/log.h>
#include <svc/sharedmem.h>
#include <svc/svc.h>
#include <wfb/wfb_rx.h>
#include <wfb/wfb_status.h>

#include <private/rhex_telemetry_rx.h>

typedef struct {
	uint32_t seq;
	uint16_t len;
	uint8_t filled;
	char payload[400];
} buffer_t;

#define BUF_SZ (100U)

static buffer_t buffer[BUF_SZ];
static size_t buf_pos = BUF_SZ - 2U;

// uint32_t seqbuffer[5];

uint32_t seqno_telemetry = 0;
uint8_t seqno_rc = 0;
uint32_t seqnolast_telemetry = 0;
uint8_t seqnolast_rc = 0;

int seqnumplayed = 0;

int telemetry_received_yet = 0;
int rc_received_yet = 0;

int port_encoded;

wifibroadcast_rx_status_t_rc *rx_status_telemetry = NULL;

static shm_t rx_status_telemetry_shm;

uint16_t sumdcrc = 0;
uint16_t ibuschecksum = 0;

uint32_t lastseq;

/*
 * Telemetry frame header consisting of seqnr and payload length
 */
struct header_s {
	uint32_t seqnumber;
	uint16_t length;
	uint8_t data[1U];
};

static void
process_packet(wfb_rx_packet_t *rx_data)
{
	do {
		/* write statistics */
		rx_status_telemetry->adapter[rx_data->adapter].current_signal_dbm = rx_data->dbm;
		rx_status_telemetry->adapter[rx_data->adapter].received_packet_cnt++;
		rx_status_telemetry->last_update = svc_get_monotime();
		shm_map_write(&rx_status_telemetry_shm, &rx_status_telemetry,
			      sizeof(rx_status_telemetry));

		struct header_s *header = (struct header_s *)rx_data->data;

		bool already_received = false;

		size_t i;
		for (i = 0; i < BUF_SZ; i++) {
			if (buffer[i].seq == header->seqnumber) {
				/* Seqnum has already been received */
				already_received = 1;
				log_dbg("seq %d dup", header->seqnumber);
				break;
			}
		}
		if (already_received) {
			break;
		}

		if ((header->seqnumber - lastseq) > (UINT32_MAX / 2U)) {
			/* received old seqno */
			break;
		}

		memcpy(buffer[buf_pos].payload, header->data, header->length);

		buffer[buf_pos].len = header->length;
		buffer[buf_pos].seq = header->seqnumber;
		buffer[buf_pos].filled = 1;

		log_dbg("seq %u->buf[%u] ", buffer[buf_pos].seq, buf_pos);

		buf_pos++;
		if (buf_pos >= BUF_SZ) {
			buf_pos = 0U;
		}
	} while (false);
}

static void
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

int
telemetry_rx_init(void)
{
	int result = -1;

	do {
		if (!shm_map_init("shm_rx_status_rc", sizeof(wifibroadcast_rx_status_t_rc))) {
			break;
		}

		if (!shm_map_open("shm_rx_status_rc", &rx_status_telemetry_shm)) {
			break;
		}

		status_memory_init_rc(rx_status_telemetry);

		shm_map_write(&rx_status_telemetry_shm, &rx_status_telemetry,
			      sizeof(rx_status_telemetry));

		result = 0;
	} while (false);

	return result;
}

int
telemetry_rx_main(void)
{
	log_dbg("RX R/C Telemetry_buf started");

	int result;

	int p;

	for (p = 0; p < 100; p++) {
		buffer[p].filled = 0;
		buffer[p].seq = 0;
	}

	wfb_rx_t telemetry_rx = {
	    0,
	};

	result = wfb_rx_init(&telemetry_rx, 1);
	if (result != 0) {
		return result;
	}

	rx_status_telemetry->wifi_adapter_cnt = telemetry_rx.count;

	log_inf("wifi_adapter_cnt: %u", telemetry_rx.count);

	lastseq = 0;

	while (svc_cycle()) {
		wfb_rx_packet_t rx_data = {
		    0,
		};

		int r = wfb_rx_packet(&telemetry_rx, &rx_data);

		if (r < 0) {
			result = -1;
			break;
		}

		if (r > 0) {
			process_packet(&rx_data);
		}
	}

	return result;
}
