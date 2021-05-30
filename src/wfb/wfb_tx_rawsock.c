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

#include <net/if.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <log.h>
#include <svc_context.h>
#include <wfb/fec.h>
#include <wfb/wfb_tx_rawsock.h>

#define MAX_PACKET_LENGTH 4192
#define MAX_USER_PACKET_LENGTH 2278
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32

#define IEEE80211_RADIOTAP_MCS_HAVE_BW 0x01
#define IEEE80211_RADIOTAP_MCS_HAVE_MCS 0x02
#define IEEE80211_RADIOTAP_MCS_HAVE_GI 0x04
#define IEEE80211_RADIOTAP_MCS_HAVE_FMT 0x08

#define IEEE80211_RADIOTAP_MCS_BW_20 0
#define IEEE80211_RADIOTAP_MCS_BW_40 1
#define IEEE80211_RADIOTAP_MCS_BW_20L 2
#define IEEE80211_RADIOTAP_MCS_BW_20U 3
#define IEEE80211_RADIOTAP_MCS_SGI 0x04
#define IEEE80211_RADIOTAP_MCS_FMT_GF 0x08
#define IEEE80211_RADIOTAP_MCS_HAVE_FEC 0x10
#define IEEE80211_RADIOTAP_MCS_HAVE_STBC 0x20

#define IEEE80211_RADIOTAP_MCS_FEC_LDPC 0x10
#define IEEE80211_RADIOTAP_MCS_STBC_MASK 0x60
#define IEEE80211_RADIOTAP_MCS_STBC_1 1
#define IEEE80211_RADIOTAP_MCS_STBC_2 2
#define IEEE80211_RADIOTAP_MCS_STBC_3 3
#define IEEE80211_RADIOTAP_MCS_STBC_SHIFT 5

static size_t param_data_packets_per_block = 8U;
static size_t param_fec_packets_per_block = 4U;
static size_t param_packet_length = 1024U;
static size_t param_min_packet_length = 24U;
static size_t param_measure = 0U;

static uint8_t packet_transmit_buffer[MAX_PACKET_LENGTH];

static int skipfec = 0;
static int block_cnt = 0;

static long long took_last = 0;
static long long took = 0;

static long long injection_time_now = 0;
static long long injection_time_prev = 0;

/*
 * This sits at the payload of the wifi packet (outside of FEC)
 *
 */
typedef struct {
	uint32_t sequence_number;
} __attribute__((packed)) wifi_packet_header_t;

/*
 * This sits at the data payload (which is usually right after the wifi_packet_header_t)
 * (inside of FEC)
 */
typedef struct {
	uint32_t data_length;
} __attribute__((packed)) payload_header_t;

static u8 u8aRadiotapHeader80211N[] __attribute__((unused)) = {
    0x00, 0x00,		    // <-- radiotap version
    0x0d, 0x00,		    // <- radiotap header length
    0x00, 0x80, 0x08, 0x00, // <-- radiotap present flags:  RADIOTAP_TX_FLAGS + RADIOTAP_MCS
    0x08, 0x00,		    // RADIOTAP_F_TX_NOACK
    0,	  0,	0	    // bitmap, flags, mcs_index
};

static u8 u8aRadiotapHeader[] = {
    0x00, 0x00,		    // <-- radiotap version
    0x0c, 0x00,		    // <- radiotap header length
    0x04, 0x80, 0x00, 0x00, // <-- radiotap present flags (rate + tx flags)
    0x00,		    // datarate (will be overwritten later in packet_header_init)
    0x00,		    // ??
    0x00, 0x00		    // ??
};

static u8 u8aIeeeHeader_data_short[] = {
    0x08, 0x01, 0x00, 0x00, // frame control field (2bytes), duration (2 bytes)
    0xff // port =  1st byte of IEEE802.11 RA (mac) must be something odd (wifi hardware determines
	 // broadcast/multicast through odd/even check)
};

static u8 u8aIeeeHeader_data[] = {
    0x08, 0x02, 0x00, 0x00, // frame control field (2bytes), duration (2 bytes)
    0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, // port = 1st byte of IEEE802.11 RA (mac) must be something odd (wifi hardware
	  // determines broadcast/multicast through odd/even check)
    0x13, 0x22, 0x33, 0x44, 0x55, 0x66, // mac
    0x13, 0x22, 0x33, 0x44, 0x55, 0x66, // mac
    0x00, 0x00 // IEEE802.11 seqnum, (will be overwritten later by Atheros firmware/wifi chip)
};

static u8 u8aIeeeHeader_rts[] = {
    0xb4, 0x01, 0x00, 0x00, // frame control field (2 bytes), duration (2 bytes)
    0xff, // port = 1st byte of IEEE802.11 RA (mac) must be something odd (wifi hardware determines
	  // broadcast/multicast through odd/even check)
};

/*=========================================================*/
/* copied from OpenHD */
static void
lib_init_packet_buffer(packet_buffer_t *p)
{
	p->valid = 0;
	p->crc_correct = 0;
	p->len = 0;
	p->data = NULL;
}

static void
alloc_packet_buffer(packet_buffer_t *p, size_t len)
{
	p->len = 0;
	p->data = (uint8_t *)malloc(len);
}

static packet_buffer_t *
alloc_packet_buffer_list(size_t num_packets, size_t packet_length)
{
	packet_buffer_t *retval;
	size_t i;

	retval = (packet_buffer_t *)malloc(sizeof(packet_buffer_t) * num_packets);

	for (i = 0; i < num_packets; ++i) {
		lib_init_packet_buffer(retval + i);
		alloc_packet_buffer(retval + i, packet_length);
	}

	return retval;
}

/*=========================================================*/

static int
wfb_open_rawsock(const char *ifname)
{
	int sock;

	sock = wfb_open_sock(ifname);
	if (sock == -1) {
		exit(1);
	}

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 8000;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
		log_err("setsockopt SO_SNDTIMEO");
	}

	int sendbuff = 131072;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)) < 0) {
		log_err("setsockopt SO_SNDBUF");
	}

	return sock;
}

static int
packet_header_init80211N(uint8_t *packet_header, int type, int port)
{
	u8 *pu8 = packet_header;

	int port_encoded = 0;

	memcpy(packet_header, u8aRadiotapHeader80211N, sizeof(u8aRadiotapHeader80211N));
	pu8 += sizeof(u8aRadiotapHeader80211N);

	switch (type) {
	case 0:
		/* Short DATA frame */
		log_inf("using short DATA frames");

		port_encoded = (port * 2) + 1;

		/* First byte of RA mac is the port */
		u8aIeeeHeader_data_short[4] = port_encoded;

		/* Copy data short header to pu8 */
		memcpy(pu8, u8aIeeeHeader_data_short, sizeof(u8aIeeeHeader_data_short));
		pu8 += sizeof(u8aIeeeHeader_data_short);

		break;

	case 1:
		/* Standard DATA frame */
		log_inf("using standard DATA frames");

		port_encoded = (port * 2) + 1;

		/* First byte of RA mac is the port */
		u8aIeeeHeader_data[4] = port_encoded;

		/* Copy data header to pu8 */
		memcpy(pu8, u8aIeeeHeader_data, sizeof(u8aIeeeHeader_data));
		pu8 += sizeof(u8aIeeeHeader_data);

		break;

	case 2:
		/* RTS frame */
		fprintf(stderr, "using RTS frames\n");

		port_encoded = (port * 2) + 1;

		/* First byte of RA mac is the port */
		u8aIeeeHeader_rts[4] = port_encoded;

		/* Copy RTS header to pu8 */
		memcpy(pu8, u8aIeeeHeader_rts, sizeof(u8aIeeeHeader_rts));
		pu8 += sizeof(u8aIeeeHeader_rts);

		break;

	default:
		log_err("Wrong or no frame type specified");
		exit(1);
		break;
	}

	/* The length of the header */
	return pu8 - packet_header;
}

int
packet_header_init(uint8_t *packet_header, int type, int rate, int port)
{
	u8 *pu8 = packet_header;

	int port_encoded = 0;

	switch (rate) {
	case 1:
		u8aRadiotapHeader[8] = 0x02;
		break;

	case 2:
		u8aRadiotapHeader[8] = 0x04;
		break;

	case 5:
		// 5.5
		u8aRadiotapHeader[8] = 0x0b;
		break;

	case 6:
		u8aRadiotapHeader[8] = 0x0c;
		break;

	case 11:
		u8aRadiotapHeader[8] = 0x16;
		break;

	case 12:
		u8aRadiotapHeader[8] = 0x18;
		break;

	case 18:
		u8aRadiotapHeader[8] = 0x24;
		break;

	case 24:
		u8aRadiotapHeader[8] = 0x30;
		break;

	case 36:
		u8aRadiotapHeader[8] = 0x48;
		break;

	case 48:
		u8aRadiotapHeader[8] = 0x60;
		break;

	default:
		log_err("Wrong or no data rate specified");
		exit(1);

		break;
	}

	memcpy(packet_header, u8aRadiotapHeader, sizeof(u8aRadiotapHeader));
	pu8 += sizeof(u8aRadiotapHeader);

	switch (type) {
	case 0:
		/* Short DATA frame */
		log_inf("using short DATA frames");

		port_encoded = (port * 2) + 1;

		/* First byte of RA mac is the port */
		u8aIeeeHeader_data_short[4] = port_encoded;

		/* Copy data short header to pu8 */
		memcpy(pu8, u8aIeeeHeader_data_short, sizeof(u8aIeeeHeader_data_short));
		pu8 += sizeof(u8aIeeeHeader_data_short);

		break;

	case 1:
		/* Standard DATA frame */
		log_inf("using standard DATA frames");

		port_encoded = (port * 2) + 1;

		/* First byte of RA mac is the port */
		u8aIeeeHeader_data[4] = port_encoded;

		/* Copy data header to pu8 */
		memcpy(pu8, u8aIeeeHeader_data, sizeof(u8aIeeeHeader_data));
		pu8 += sizeof(u8aIeeeHeader_data);

		break;

	case 2:
		/* RTS frame */
		log_inf("using RTS frames");

		port_encoded = (port * 2) + 1;

		/* First byte of RA mac is the port */
		u8aIeeeHeader_rts[4] = port_encoded;

		/* Copy RTS header to pu8 */
		memcpy(pu8, u8aIeeeHeader_rts, sizeof(u8aIeeeHeader_rts));
		pu8 += sizeof(u8aIeeeHeader_rts);

		break;

	default:
		log_err("Wrong or no frame type specified");

		exit(1);

		break;
	}

	/*
	 * The length of just the header
	 */
	return pu8 - packet_header;
}

static int
pb_transmit_packet(wfb_tx_t *wfb_tx, int seq_nr, uint8_t *packet_transmit_buffer,
		   int packet_header_len, const uint8_t *packet_data, int packet_length)
{
	/* Add header outside of FEC */
	wifi_packet_header_t *wph =
	    (wifi_packet_header_t *)(packet_transmit_buffer + packet_header_len);

	wph->sequence_number = seq_nr;

	memcpy(packet_transmit_buffer + packet_header_len + sizeof(wifi_packet_header_t),
	       packet_data, packet_length);

	int plen = packet_length + packet_header_len + sizeof(wifi_packet_header_t);

	size_t i = 0;
	for (i = 0; i < wfb_tx->count; i++) {
		if (write(wfb_tx->sock[i], packet_transmit_buffer, plen) < 0) {
			return 1;
		}
	}

	return 0;
}

static void
pb_transmit_block(wfb_tx_t *wfb_tx, packet_buffer_t *pbl, int *seq_nr, int packet_length,
		  uint8_t *packet_transmit_buffer, int packet_header_len,
		  int data_packets_per_block, int fec_packets_per_block)
{
	uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
	uint8_t fec_pool[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK][MAX_USER_PACKET_LENGTH];
	uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];

	int i;
	for (i = 0; i < data_packets_per_block; ++i) {
		data_blocks[i] = pbl[i].data;
	}

	/*
	 * This allows the number of FEC packets to be set to zero
	 *
	 * In that case the FEC process will not be run at all, and only data blocks will be
	 * transmitted
	 */
	if (fec_packets_per_block) {
		for (i = 0; i < fec_packets_per_block; ++i) {
			fec_blocks[i] = fec_pool[i];
		}
		fec_encode(packet_length, data_blocks, data_packets_per_block,
			   (unsigned char **)fec_blocks, fec_packets_per_block);
	}

	uint8_t *pb = packet_transmit_buffer;
	pb += packet_header_len;

	int di = 0;
	int fi = 0;
	int seq_nr_tmp = *seq_nr;
	int counterfec = 0;

	long long prev_time = svc_get_time();

	/*
	 * Send data and FEC packets interleaved
	 */
	while ((di < data_packets_per_block) || (fi < fec_packets_per_block)) {
		if (di < data_packets_per_block) {
			if (pb_transmit_packet(wfb_tx, seq_nr_tmp, packet_transmit_buffer,
					       packet_header_len, data_blocks[di], packet_length)) {
				log_warn("packet send failed");
			}

			seq_nr_tmp++;
			di++;
		}

		if (fi < fec_packets_per_block) {
			if (skipfec < 1) {
				if (pb_transmit_packet(wfb_tx, seq_nr_tmp, packet_transmit_buffer,
						       packet_header_len, fec_blocks[fi],
						       packet_length)) {
					// td1->tx_status->injection_fail_cnt++;
					log_warn("packet send failed");
				}
			} else {
				if (counterfec % 2 == 0) {
					if (pb_transmit_packet(
						wfb_tx, seq_nr_tmp, packet_transmit_buffer,
						packet_header_len, fec_blocks[fi], packet_length)) {
						// td1->tx_status->injection_fail_cnt++;
						log_warn("packet send failed");
					}
				} else {
					// fprintf(stderr, "not transmitted\n");
				}

				counterfec++;
			}

			seq_nr_tmp++;
			fi++;
		}

		skipfec--;
	}

	/*
	 * We don't do any of this during a bandwidth measurement, because it would affect the
	 * measurement and cause the air side to believe there is more bandwidth available than
	 * there really is.
	 *
	 * Before this check was added, RTL8812au cards were measuring upwards of 40Mbit available
	 * bandwidth, which is clearly wrong when the hardware data rate is only 18Mbit.
	 */
	if (param_measure == 0) {
		block_cnt++;

		// td1->tx_status->injected_block_cnt++;

		took_last = took;
		took = svc_get_time() - prev_time;

		// if (took > 50) fprintf(stderr, "write took %lldus\n", took);

		if (took >
		    (packet_length * (data_packets_per_block + fec_packets_per_block)) / 1.5) {
			/*
			 * We simply assume 1us per byte = 1ms per 1024 byte packet (not very exact
			 * ...)
			 */
			skipfec = 4;
			// td1->tx_status->skipped_fec_cnt = td1->tx_status->skipped_fec_cnt +
			// skipfec;

			// fprintf(stderr, "\nwrite took %lldus skipping FEC packets ...\n", took);
		}

		if (block_cnt % 50 == 0 && param_measure == 0) {
			/*fprintf(stderr,
				"\t\t%d blocks sent, injection time per block %lldus, %d fecs "
				"skipped, %d packet injections failed.          \r",
				block_cnt, td1->tx_status->injection_time_block,
				td1->tx_status->skipped_fec_cnt,
				td1->tx_status->injection_fail_cnt);
			fflush(stderr);*/
		}

		if (took < took_last) {
			/*
			 * If we have a lower injection_time than last time, ignore
			 */
			took = took_last;
		}

		injection_time_now = svc_get_time();

		if (injection_time_now - injection_time_prev > 220) {
			injection_time_prev = svc_get_time();
			// td1->tx_status->injection_time_block = took;
			took = 0;
			took_last = 0;
		}
	}

	*seq_nr += data_packets_per_block + fec_packets_per_block;

	/*
	 * Reset the length for the next packet
	 */
	for (i = 0; i < data_packets_per_block; ++i) {
		pbl[i].len = 0;
	}
}

int
wfb_stream_init(wfb_tx_rawsock_t *wfb_stream, size_t num_if, const if_desc_t interfaces[],
		uint8_t tx_buf[], int port, int packet_type, bool useMCS, bool useSTBC,
		bool useLDPC)
{
	int param_data_rate = 12;
	size_t i;
	size_t num_interfaces = 0U;

	if (useMCS) {
		log_inf("Using 802.11N mode");

		u8 mcs_flags = 0;
		u8 mcs_known = (IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_BW |
				IEEE80211_RADIOTAP_MCS_HAVE_GI | IEEE80211_RADIOTAP_MCS_HAVE_STBC |
				IEEE80211_RADIOTAP_MCS_HAVE_FEC);

		if (useSTBC == 1) {
			fprintf(stderr, "STBC enabled\n");
			mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_STBC_1
						    << IEEE80211_RADIOTAP_MCS_STBC_SHIFT;
		}

		if (useLDPC == 1) {
			fprintf(stderr, "LDPC enabled\n");
			mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_FEC_LDPC;
		}

		u8aRadiotapHeader80211N[10] = mcs_known;
		u8aRadiotapHeader80211N[11] = mcs_flags;
		u8aRadiotapHeader80211N[12] = param_data_rate;

		wfb_stream->stream_phdr_len = packet_header_init80211N(tx_buf, packet_type, port);
	} else {
		wfb_stream->stream_phdr_len =
		    packet_header_init(tx_buf, packet_type, param_data_rate, port);
	}

	wfb_stream->input_buffer.seq_nr = 0;
	wfb_stream->input_buffer.curr_pb = 0;
	wfb_stream->input_buffer.pbl =
	    alloc_packet_buffer_list(param_data_packets_per_block, MAX_PACKET_LENGTH);

	wfb_stream->port = port;

	/*
	 * Prepare the buffers with headers
	 */
	for (i = 0; i < param_data_packets_per_block; ++i) {
		wfb_stream->input_buffer.pbl[i].len = 0;
	}

	fec_init();

	/*
	 * Initialize telemetry shared mem for rssi based transmission (-y 1)
	 */
	/*telemetry_data_t td;
	telemetry_init(&td);*/

	for (i = 0; (i < num_if) && (num_interfaces < MAX_ADAP); i++) {
		wfb_stream->wfb_tx.sock[wfb_stream->wfb_tx.count] =
		    wfb_open_rawsock(interfaces[i].ifname);
		wfb_stream->wfb_tx.count++;

		/*
		 * Wait a bit between configuring interfaces to reduce Atheros and Pi USB flakiness
		 */
		usleep(20000);
	}

	return 0;
}

void
wfb_tx_stream(wfb_tx_rawsock_t *wfb_stream, uint8_t data[], uint16_t len)
{
	uint16_t offset = 0U;

	do {
		packet_buffer_t *pb =
		    wfb_stream->input_buffer.pbl + wfb_stream->input_buffer.curr_pb;

		/* If the buffer is fresh we add a payload header */
		if (pb->len == 0) {
			/* Make space for a length field (will be filled later) */
			pb->len += sizeof(payload_header_t);
		}

		/* распределяем данные по пакетам */
		size_t copy_len = len - offset;
		if (copy_len > (param_packet_length - pb->len)) {
			copy_len = param_packet_length - pb->len;
		}
		memcpy(&pb->data[pb->len], &data[offset], copy_len);
		offset += copy_len;

		pb->len += copy_len;

		/*
		 * Check if this packet is finished
		 */
		if (pb->len >= param_min_packet_length) {
			payload_header_t *ph = (payload_header_t *)pb->data;
			input_buffer_t *input = &wfb_stream->input_buffer;

			/*
			 * Write the length into the packet. This is needed because with FEC, we
			 * cannot use the wifi packet length anymore.
			 *
			 * We could also set the user payload to a fixed size but this would
			 * introduce additional latency since TX would need to wait until that
			 * amount of data has been received.
			 */
			ph->data_length = pb->len - sizeof(payload_header_t);
			wfb_stream->wfb_tx.pcnt++;

			/*
			 * Check if this block is finished
			 */
			if (wfb_stream->input_buffer.curr_pb == param_data_packets_per_block - 1) {
				pb_transmit_block(&wfb_stream->wfb_tx, input->pbl, &(input->seq_nr),
						  param_packet_length, packet_transmit_buffer,
						  wfb_stream->stream_phdr_len,
						  param_data_packets_per_block,
						  param_fec_packets_per_block);
				input->curr_pb = 0;
			} else {
				input->curr_pb++;
			}
		}
	} while (offset < len);
}
