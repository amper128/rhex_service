/**
 * @file wfb_tx_rawsock.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Функции отправки потока через wifi broadcast
 */

/* Based on wifibroadcast tx by Befinitiv. GPL2 licensed. */
/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <private/radiotap.h>

#include <log/log.h>
#include <private/fec.h>
#include <svc/svc.h>
#include <wfb/wfb_rx_rawsock.h>

#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32

/*
 * This sits at the payload of the wifi packet (outside of FEC)
 *
 */
typedef struct {
	uint32_t sequence_number;
} __attribute__((packed)) wifi_packet_header_t;

/*
 * This sits at the data payload (which is usually right after the wifi_packet_header_t) (inside of
 * FEC)
 *
 */
typedef struct {
	uint32_t data_length;
} __attribute__((packed)) payload_header_t;

struct payload_data_t {
	const uint8_t *data;
	size_t size;
	bool crc_ok;
};

static const size_t param_data_packets_per_block = 8U;
static const size_t param_fec_packets_per_block = 4U;
static const size_t param_block_buffers = 1U;
static const size_t param_packet_length = 1024U;

static int max_block_num = -1;

static uint64_t prev_time = 0ULL;
static uint64_t now = 0ULL;

static uint64_t pm_prev_time = 0;
static uint64_t pm_now = 0;

static uint32_t packets_missing;
static uint32_t packets_missing_last;

static int dbm[NL_MAX_IFACES];
static int dbm_last[NL_MAX_IFACES];

static uint64_t dbm_ts_prev[NL_MAX_IFACES];
static uint64_t dbm_ts_now[NL_MAX_IFACES];

/*=========================================================*/
/* copied from OpenHD */
static void
init_packet_buffer(packet_buffer_t *p)
{
	p->valid = false;
	p->crc_correct = false;
	p->len = 0U;
	p->data = NULL;
}

static void
alloc_packet_buffer(packet_buffer_t *p, size_t len)
{
	p->len = 0U;
	p->data = (uint8_t *)malloc(len);
}

static packet_buffer_t *
alloc_packet_buffer_list(size_t num_packets, size_t packet_length)
{
	packet_buffer_t *pb;
	size_t i;

	pb = (packet_buffer_t *)malloc(sizeof(packet_buffer_t) * num_packets);

	for (i = 0U; i < num_packets; i++) {
		init_packet_buffer(&pb[i]);
		alloc_packet_buffer(&pb[i], packet_length);
	}

	return pb;
}

/*=========================================================*/

static void
block_buffer_list_reset(block_buffer_t *block_buffer_list, size_t block_buffer_list_len)
{
	size_t i;
	block_buffer_t *rb = block_buffer_list;

	for (i = 0U; i < block_buffer_list_len; i++) {
		rb->block_num = -1;

		packet_buffer_t *p = rb->packet_buffer_list;

		size_t j;
		for (j = 0; j < param_data_packets_per_block + param_fec_packets_per_block; j++) {
			p->valid = false;
			p->crc_correct = false;
			p->len = 0U;
			p++;
		}

		rb++;
	}
}

static void
process_payload(wfb_rx_stream_t *rx, const struct payload_data_t *pd,
		block_buffer_t *block_buffer_list, wfb_rx_stream_packet_t *rx_data)
{
	const wifi_packet_header_t *wph;

	int block_num;
	int packet_num;
	size_t i;
	size_t kbitrate = 0U;

	wph = (wifi_packet_header_t *)pd->data;
	const char *data = (const char *)&wph[1U];
	size_t data_len = pd->size - sizeof(wifi_packet_header_t);

	/*
	 * If aram_data_packets_per_block+param_fec_packets_per_block would be limited to powers of
	 * two, this could be replaced by a logical AND operation
	 */
	block_num =
	    wph->sequence_number / (param_data_packets_per_block + param_fec_packets_per_block);

	// log_dbg("adap %d rec %x blk %x crc %d len %d", adapter_no, wph->sequence_number,
	// block_num, crc_correct, data_len);

	/*
	 * We have received a block number that exceeds the block numbers we have seen so far
	 *
	 * We need to make room for this new block, or we have received a block_num that is
	 * several times smaller than the current window of buffers.
	 *
	 * This indicates that either the window is too small, or that the transmitter has been
	 * restarted
	 */
	bool tx_restart = (int)((size_t)block_num + (128U * param_block_buffers)) < max_block_num;

	if (((block_num > max_block_num) || tx_restart) && pd->crc_ok) {
		if (tx_restart) {
			rx->rx_status.tx_restart_cnt++;
			rx->rx_status.received_block_cnt = 0U;
			rx->rx_status.damaged_block_cnt = 0U;
			rx->rx_status.received_packet_cnt = 0U;
			rx->rx_status.lost_packet_cnt = 0U;
			rx->rx_status.kbitrate = 0U;

			size_t g;
			for (g = 0; g < NL_MAX_IFACES; g++) {
				rx->rx_status.adapter[g].received_packet_cnt = 0U;
				rx->rx_status.adapter[g].wrong_crc_cnt = 0U;
				rx->rx_status.adapter[g].current_signal_dbm = -126;
				rx->rx_status.adapter[g].signal_good = 0U;
			}

			log_inf("TX re-start detected");
			block_buffer_list_reset(block_buffer_list, param_block_buffers);
		}

		/*
		 * First, find the minimum block num in the buffers list. this will be the block
		 * that we replace
		 */
		int min_block_num = INT32_MAX;
		int min_block_num_idx = 0;

		for (i = 0U; i < param_block_buffers; i++) {
			if (block_buffer_list[i].block_num < min_block_num) {
				min_block_num = block_buffer_list[i].block_num;
				min_block_num_idx = i;
			}
		}

		/*log_dbg("removing block %x at index %i for block %x", min_block_num,
			min_block_num_idx, block_num);*/

		packet_buffer_t *packet_buffer_list =
		    block_buffer_list[min_block_num_idx].packet_buffer_list;

		int last_block_num = block_buffer_list[min_block_num_idx].block_num;

		if (last_block_num != -1) {
			rx->rx_status.received_block_cnt++;

			/*
			 * We have pointers to the packet buffers (to get information about CRC and
			 * vadility), and raw data pointers for fec_decode
			 */
			packet_buffer_t *data_pkgs[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
			packet_buffer_t *fec_pkgs[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
			uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
			uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];

			int datas_missing = 0, datas_corrupt = 0, fecs_missing = 0,
			    fecs_corrupt = 0;
			size_t di = 0U, fi = 0U;

			/*
			 * First, split the received packets into data and FEC packets, and count
			 * the damaged packets
			 */

			i = 0U;
			while ((di < param_data_packets_per_block) ||
			       (fi < param_fec_packets_per_block)) {
				if (di < param_data_packets_per_block) {
					data_pkgs[di] = packet_buffer_list + i++;
					data_blocks[di] = data_pkgs[di]->data;

					if (!data_pkgs[di]->valid) {
						datas_missing++;
					}

					// if(data_pkgs[di]->valid && !data_pkgs[di]->crc_correct)
					// datas_corrupt++; // not needed as we dont receive fcs
					// fail frames

					di++;
				}

				if (fi < param_fec_packets_per_block) {
					fec_pkgs[fi] = packet_buffer_list + i++;

					if (!fec_pkgs[fi]->valid) {
						fecs_missing++;
					}

					// if(fec_pkgs[fi]->valid && !fec_pkgs[fi]->crc_correct)
					// fecs_corrupt++; // not needed as we dont receive fcs fail
					// frames

					fi++;
				}
			}

			const int good_fecs_c =
			    (int)param_fec_packets_per_block - fecs_missing - fecs_corrupt;
			const int datas_missing_c = datas_missing;
			const int datas_corrupt_c = datas_corrupt;
			const int fecs_missing_c = fecs_missing;
			// const int fecs_corrupt_c = fecs_corrupt;

			uint32_t packets_lost_in_block = 0U;
			// int good_fecs = good_fecs_c;

			/*
			 * The following three fields are infos for fec_decode
			 */
			unsigned int fec_block_nos[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
			unsigned int erased_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
			unsigned int nr_fec_blocks = 0;

			if ((datas_missing_c + fecs_missing_c) > 0) {
				packets_lost_in_block =
				    (uint32_t)(datas_missing_c + fecs_missing_c);
				rx->rx_status.lost_packet_cnt += (uint32_t)packets_lost_in_block;
			}

			rx->rx_status.received_packet_cnt += param_data_packets_per_block +
							     param_fec_packets_per_block -
							     packets_lost_in_block;

			packets_missing_last = packets_missing;
			packets_missing = packets_lost_in_block;

			if (packets_missing < packets_missing_last) {
				/*
				 * If we have less missing packets than last time, ignore
				 */
				packets_missing = packets_missing_last;
			}

			pm_now = svc_get_monotime();

			if ((pm_now - pm_prev_time) > (220 * TIME_MS)) {
				pm_prev_time = svc_get_monotime();
				rx->rx_status.lost_per_block_cnt = packets_missing;
				packets_missing = 0;
				packets_missing_last = 0;
			}

			fi = 0;
			di = 0;

			/*
			 * Look for missing DATA and replace them with good FECs
			 */
			while ((di < param_data_packets_per_block) &&
			       (fi < param_fec_packets_per_block)) {
				/*
				 * If this data is fine, we go to the next
				 */
				if (data_pkgs[di]->valid && data_pkgs[di]->crc_correct) {
					di++;
					continue;
				}

				/*
				 * If this DATA is corrupt and there are less good fecs than missing
				 * datas we cannot do anything for this data
				 *
				 * Not needed right now, as we dont receive FCS failure frames from
				 * the NIC anyway
				 */

				/*
		if (data_pkgs[di]->valid && !data_pkgs[di]->crc_correct && good_fecs <=
		datas_missing) { di++; continue;
		}
		*/

				/*
				 * If this FEC is not received, we go on to the next
				 */
				if (!fec_pkgs[fi]->valid) {
					fi++;

					continue;
				}

				/*
				 * If this FEC is corrupted and there are more lost packages than
				 * good fecs we should replace this DATA even with this corrupted
				 * FEC
				 *
				 * Not needed right now, as we dont receive FCS failure frames from
				 * the NIC anyway
				 */

				/*
		if (!fec_pkgs[fi]->crc_correct && datas_missing > good_fecs) {
		    fi++;
		    continue;
		}
		*/

				if (!data_pkgs[di]->valid) {
					datas_missing--;
				}
				/* not needed as we dont receive fcs fail frames
		else if (!data_pkgs[di]->crc_correct) {
		    datas_corrupt--;
		}
		*/

				/*
				 * Not needed as we dont receive fcs fail frames
				 */

				/*
		if(fec_pkgs[fi]->crc_correct) {
		    good_fecs--;
		}
		*/

				/*
				 * At this point, data is invalid and fec is good -> replace data
				 * with fec
				 */
				erased_blocks[nr_fec_blocks] = di;
				fec_block_nos[nr_fec_blocks] = fi;
				fec_blocks[nr_fec_blocks] = fec_pkgs[fi]->data;

				di++;
				fi++;
				nr_fec_blocks++;
			}

			int reconstruction_failed = datas_missing_c + datas_corrupt_c > good_fecs_c;
			if (reconstruction_failed) {
				/*
				 * We did not have enough FEC packets to repair this block
				 */
				rx->rx_status.damaged_block_cnt++;
				// fprintf(stderr, "Could not fully reconstruct block %x! Damage
				// rate: %f (%d / %d blocks)\n", last_block_num, 1.0 *
				// rx_status->damaged_block_cnt / rx_status->received_block_cnt,
				// rx_status->damaged_block_cnt, rx_status->received_block_cnt);
				// debug_print("Data mis: %d\tData corr: %d\tFEC mis: %d\tFEC corr:
				// %d\n", datas_missing_c, datas_corrupt_c, fecs_missing_c,
				// fecs_corrupt_c);
			}

			/*
			 * Decode data and write it to STDOUT
			 *
			 * This is where the video data gets moved to the rest of the system after
			 * reception
			 */
			fec_decode((unsigned int)param_packet_length, data_blocks,
				   param_data_packets_per_block, fec_blocks, fec_block_nos,
				   erased_blocks, nr_fec_blocks);

			for (i = 0U; i < param_data_packets_per_block; i++) {
				payload_header_t *ph = (payload_header_t *)data_blocks[i];

				if (!reconstruction_failed || data_pkgs[i]->valid) {
					/*
					 * If reconstruction fails, the data_length value is
					 * undefined
					 *
					 * Limit it to some sensible value
					 */
					if (ph->data_length > param_packet_length) {
						ph->data_length = param_packet_length;
					}

					memcpy(&rx_data->data[rx_data->bytes],
					       data_blocks[i] + sizeof(payload_header_t),
					       ph->data_length);
					rx_data->bytes += (int)ph->data_length;

					// write(STDOUT_FILENO, data_blocks[i] +
					// sizeof(payload_header_t), ph->data_length);
					// fflush(stdout);

					now = svc_get_monotime();

					rx->bytes_decoded += ph->data_length;

					if ((now - prev_time) > (500ULL * TIME_MS)) {
						prev_time = svc_get_monotime();

						kbitrate = ((rx->bytes_decoded * 8) / 1024) * 2;
						rx->rx_status.kbitrate = kbitrate;
						rx->bytes_decoded = 0;

						// log_dbg("kbitrate: %d", kbitrate);
					}
				}
			}

			/*
			 * Reset buffers
			 */
			for (i = 0; i < param_data_packets_per_block + param_fec_packets_per_block;
			     i++) {
				packet_buffer_t *p = packet_buffer_list + i;
				p->valid = 0;
				p->crc_correct = 0;
				p->len = 0;
			}
		}

		block_buffer_list[min_block_num_idx].block_num = block_num;
		max_block_num = block_num;
	}

	/*
	 * Find the buffer into which we have to write this packet
	 */
	block_buffer_t *rbb = block_buffer_list;
	for (i = 0; i < param_block_buffers; i++) {
		if (rbb->block_num == block_num) {
			break;
		}
		rbb++;
	}

	/*
	 * Check if we have actually found the corresponding block. this could not be the case due
	 * to a corrupt packet
	 */
	if (i != param_block_buffers) {
		packet_buffer_t *pbl = rbb->packet_buffer_list;

		/*
		 * If retr_block_size would be limited to powers of two, this could be replace by a
		 * locical and operation
		 */
		packet_num = wph->sequence_number %
			     (param_data_packets_per_block + param_fec_packets_per_block);

		/*
		 * Only overwrite packets where the checksum is not yet correct. otherwise the
		 * packets are already received correctly
		 */
		if (pbl[packet_num].crc_correct == 0) {
			memcpy(pbl[packet_num].data, data, data_len);
			pbl[packet_num].len = data_len;
			pbl[packet_num].valid = 1;
			pbl[packet_num].crc_correct = pd->crc_ok;

			/// fprintf(stderr, "rx INFO:
			/// pbl[packet_numer].crc_correct=0");
		}
	}
}

static int
wfb_rx_stream_interface(wfb_rx_stream_t *rx, wfb_rx_stream_packet_t *rx_data, size_t adapter_no)
{
	int result = 0;

	monitor_interface_t *interface = &rx->wfb_rx.iface[adapter_no];

	struct pcap_pkthdr *ppcapPacketHeader = NULL;

	struct ieee80211_radiotap_iterator rti;
	// PENUMBRA_RADIOTAP_DATA prd;
	uint8_t *pu8Payload = NULL;
	ssize_t bytes;
	int retval;
	size_t u16HeaderLen;
	struct payload_data_t pd;

	// receive
	retval = pcap_next_ex(interface->ppcap, &ppcapPacketHeader, (const u_char **)&pu8Payload);
	if (retval < 0) {
		if (strcmp("The interface went down", pcap_geterr(interface->ppcap)) == 0) {
			log_err("rx: The interface went down");
			exit(9);
		} else {
			log_err("rx: %s", pcap_geterr(interface->ppcap));
			exit(2);
		}
	}

	if (retval != 1) {
		// exit(1);
		return 1;
	}

	/*
	 * Fetch radiotap header length from radiotap header
	 *
	 * Atheros: 36
	 * Ralink: 18
	 */
	u16HeaderLen = (size_t)(pu8Payload[2] + (pu8Payload[3] << 8));

	switch (pu8Payload[u16HeaderLen + 1]) {
	case 0x01:
		/* Data short, RTS telemetry */
		// log_dbg("Data short or RTS telemetry frame");
		interface->n80211HeaderLength = 0x05;
		break;
	case 0x02:
		/* Data telemetry */
		// log_dbg("Data telemetry frame");
		interface->n80211HeaderLength = 0x18;
		break;
	default:
		break;
	}

	// log_dbg("ppcapPacketHeader->len: %d", ppcapPacketHeader->len);
	if (ppcapPacketHeader->len < (bpf_u_int32)(u16HeaderLen + interface->n80211HeaderLength)) {
		log_err("ppcapheaderlen < u16headerlen+n80211headerlen: "
			"ppcapPacketHeader->len: %d",
			ppcapPacketHeader->len);
		exit(1);
	}

	bytes = (ssize_t)(ppcapPacketHeader->len - (u16HeaderLen + interface->n80211HeaderLength));
	// log_dbg("bytes: %d", bytes);
	if (bytes < 0) {
		log_err("bytes < 0: bytes: %d", bytes);
		exit(1);
	}
	pd.size = (size_t)bytes;

	if (ieee80211_radiotap_iterator_init(&rti, (struct ieee80211_radiotap_header *)pu8Payload,
					     (int)ppcapPacketHeader->len) < 0) {
		log_err("radiotap_iterator_init < 0");
		exit(1);
	}

	// int dbm = -127;

	while (ieee80211_radiotap_iterator_next(&rti) == 0) {
		switch (rti.this_arg_index) {
		case IEEE80211_RADIOTAP_RATE:
			// we don't use this radiotap info right now
			// prd.m_nRate = (*rti.this_arg);
			break;
		case IEEE80211_RADIOTAP_CHANNEL:
			// we don't use this radiotap info right now
			// prd.m_nChannel = le16_to_cpu(*((u16 *)rti.this_arg));
			// prd.m_nChannelFlags = le16_to_cpu(*((u16 *)(rti.this_arg + 2)));
			break;
		case IEEE80211_RADIOTAP_FLAGS:
			// prd.m_nRadiotapFlags = *rti.this_arg;
			break;
		case IEEE80211_RADIOTAP_ANTENNA:
			// ant[adapter_no] = (int8_t) (*rti.this_arg);
			// log_dbg("Ant: %d", ant[adapter_no]);
			break;
		case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
			// db[adapter_no] = (int8_t) (*rti.this_arg);
			// log_dbg("DB: %d", db[adapter_no]);
			break;
		case IEEE80211_RADIOTAP_DBM_ANTNOISE:
			// dbm_noise[adapter_no] = (int8_t) (*rti.this_arg);
			// log_dbg("DBM Noise: %d", dbm_noise[adapter_no]);
			break;
		case IEEE80211_RADIOTAP_LOCK_QUALITY:
			// quality[adapter_no] = (int16_t) (*rti.this_arg);
			// log_dbg("Quality: %d", quality[adapter_no]);
			break;
		case IEEE80211_RADIOTAP_TSFT:
			// tsft[adapter_no] = (int64_t) (*rti.this_arg);
			// log_dbg("TSFT: %d", tsft[adapter_no]);
			break;
		case IEEE80211_RADIOTAP_DBM_ANTSIGNAL: {
			// we don't use this radiotap info right now
			// rx_status->adapter[adapter_no].current_signal_dbm =
			// (int8_t)(*rti.this_arg);

			dbm_last[adapter_no] = dbm[adapter_no];
			dbm[adapter_no] = (int8_t)(*rti.this_arg);

			if (dbm[adapter_no] > dbm_last[adapter_no]) { // if we have a better signal
								      // than last time, ignore
				dbm[adapter_no] = dbm_last[adapter_no];
			}

			dbm_ts_now[adapter_no] = svc_get_monotime();

			if (dbm_ts_now[adapter_no] - dbm_ts_prev[adapter_no] > 220ULL * TIME_MS) {
				dbm_ts_prev[adapter_no] = svc_get_monotime();

				rx->rx_status.adapter[adapter_no].current_signal_dbm =
				    dbm[adapter_no];

				dbm[adapter_no] = 99;
				dbm_last[adapter_no] = 99;

				/*log_dbg("miss: %d   last: %d", packets_missing,
					packets_missing_last);*/
			}

		} break;

		default:
			/* do nothing */
			break;
		}
	}

	pd.data = &pu8Payload[u16HeaderLen + interface->n80211HeaderLength];

	/*
	 * Ralink and Atheros both always supply the FCS to userspace, no need to check
	 */
	// if (prd.m_nRadiotapFlags & IEEE80211_RADIOTAP_F_FCS)
	// bytes -= 4;

	/*
	 * TODO: disable checksum handling in process_payload(), not needed since we have fscfail
	 * disabled
	 */
	pd.crc_ok = true;

	rx->rx_status.adapter[adapter_no].received_packet_cnt++;

	// rx_status->adapter[adapter_no].last_update = dbm_ts_now[adapter_no];
	// fprintf(stderr,"lu[%d]: %lld\n",adapter_no,rx_status->adapter[adapter_no].last_update);
	// rx_status->adapter[adapter_no].last_update = current_timestamp();

	uint32_t best_adapter = 0U;

	uint32_t j = 0;
	uint32_t ac = rx->rx_status.wifi_adapter_cnt;
	int best_dbm = -1000;

	/*
	 * Find out which card has best signal and ignore ralink (type=1) ones
	 */
	for (j = 0U; j < ac; j++) {
		if (best_dbm < rx->rx_status.adapter[j].current_signal_dbm) {
			best_dbm = rx->rx_status.adapter[j].current_signal_dbm;
			best_adapter = j;
		}
	}

	/*
	 * Only calculate received data for the adapter with the best signal
	 */
	if (adapter_no == best_adapter) {
		rx->bytes_received += pd.size;
	}

	now = svc_get_monotime();

	if ((now - rx->current_air_datarate_ts) > (500ULL * TIME_MS)) {
		double bits_received = (rx->bytes_received * 8);
		double bits_per_second =
		    (bits_received / (now - rx->current_air_datarate_ts)) * 1000.0;

		rx->rx_status.current_air_datarate_kbit = bits_per_second / 1024;
		rx->current_air_datarate_ts = now;
		rx->bytes_received = 0U;
		shm_map_write(&rx->status_shm, &rx->rx_status, sizeof(wifibroadcast_rx_status_t));
	}

	process_payload(rx, &pd, rx->block_buffer_list, rx_data);

	return result;
}

int
wfb_rx_stream_init(wfb_rx_stream_t *rx, int port)
{
	int result = 0;

	do {
		if (!shm_map_init("shm_rx_status", sizeof(wifibroadcast_rx_status_t))) {
			result = 1;
			break;
		}

		if (!shm_map_open("shm_rx_status", &rx->status_shm)) {
			result = 1;
			break;
		}

		memset(&rx->rx_status, 0, sizeof(wifibroadcast_rx_status_t));

		result = wfb_rx_init(&rx->wfb_rx, port);
		if (result) {
			break;
		}

		rx->rx_status.wifi_adapter_cnt = rx->wfb_rx.count;

		result = shm_map_write(&rx->status_shm, &rx->rx_status,
				       sizeof(wifibroadcast_rx_status_t));
		if (result < 0) {
			break;
		}

		fec_init();

		rx->block_buffer_list = malloc(sizeof(block_buffer_t) * param_block_buffers);
		if (rx->block_buffer_list == NULL) {
			log_err("malloc() failed");
			result = 1;
			break;
		}

		size_t i;
		for (i = 0; i < param_block_buffers; i++) {
			rx->block_buffer_list[i].block_num = -1;
			rx->block_buffer_list[i].packet_buffer_list = alloc_packet_buffer_list(
			    param_data_packets_per_block + param_fec_packets_per_block,
			    MAX_PACKET_LENGTH);
		}
	} while (false);

	return result;
}

int
wfb_rx_stream(wfb_rx_stream_t *rx, wfb_rx_stream_packet_t *rx_data)
{
	int result = 0;

	size_t i;

	rx->packetcounter_ts_now[0] = svc_get_monotime();
	if (rx->packetcounter_ts_now[0] - rx->packetcounter_ts_prev[0] > 220 * TIME_MS) {
		rx->packetcounter_ts_prev[0] = svc_get_monotime();

		for (i = 0; i < rx->wfb_rx.count; i++) {
			rx->packetcounter_last[i] = rx->packetcounter[i];
			rx->packetcounter[i] = rx->rx_status.adapter[i].received_packet_cnt;

			if (rx->packetcounter[i] == rx->packetcounter_last[i]) {
				rx->rx_status.adapter[i].signal_good = 0;
			} else {
				rx->rx_status.adapter[i].signal_good = 1;
			}

			// log_dbg("signal_good[%d]: %d", i, rx->rx_status.adapter[i].signal_good);

			result = shm_map_write(&rx->status_shm, &rx->rx_status,
					       sizeof(wifibroadcast_rx_status_t));
		}
	}

	struct timeval to;
	to.tv_sec = 0;
	to.tv_usec = 1e5; // 100ms timeout
	fd_set readset;
	FD_ZERO(&readset);
	int nfds = 0;

	for (i = 0U; i < rx->wfb_rx.count; i++) {
		FD_SET(rx->wfb_rx.iface[i].selectable_fd, &readset);
		if (rx->wfb_rx.iface[i].selectable_fd > nfds) {
			nfds = rx->wfb_rx.iface[i].selectable_fd;
		}
	}

	result = select(nfds + 1, &readset, NULL, NULL, &to);

	if (result > 0) {
		for (i = 0U; i < rx->wfb_rx.count; i++) {
			if (FD_ISSET(rx->wfb_rx.iface[i].selectable_fd, &readset)) {
				result = wfb_rx_stream_interface(rx, rx_data, i);
				// rx_data->adapter = i;
				break;
			}
		}
	}

	return result;
}
