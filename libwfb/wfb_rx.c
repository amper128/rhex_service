/**
 * @file wfb_rx.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License / GPL2
 * @date 2020
 * @brief Прием данных wifi broadcast
 */

// Based on wifibroadcast rx by Befinitiv. Licensed under GPL2
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

#include <log/log.h>
#include <private/radiotap_iter.h>
#include <private/radiotap_rc.h>
#include <wfb/wfb_rx.h>

#include <pcap.h>
#include <string.h>

static const struct radiotap_align_size align_size_000000_00[] = {
    [0] =
	{
	    .align = 1,
	    .size = 4,
	},
    [52] =
	{
	    .align = 1,
	    .size = 4,
	},
};

static const struct ieee80211_radiotap_namespace vns_array[] = {
    {
	.oui = 0x000000,
	.subns = 0,
	.n_bits = sizeof(align_size_000000_00),
	.align_size = align_size_000000_00,
    },
};

static const struct ieee80211_radiotap_vendor_namespaces vns = {
    .ns = vns_array,
    .n_ns = sizeof(vns_array) / sizeof(vns_array[0]),
};

static void
open_and_configure_interface(const char name[], monitor_interface_t *interface, int port)
{
	struct bpf_program bpfprogram;
	char szProgram[512];
	char szErrbuf[PCAP_ERRBUF_SIZE];

	int port_encoded = (port * 2) + 1;

	/* open the interface in pcap */
	szErrbuf[0] = '\0';

	interface->ppcap = pcap_open_live(name, 400, 0, -1, szErrbuf);
	if (interface->ppcap == NULL) {
		log_err("Unable to open %s: %s", name, szErrbuf);
		exit(1);
	}

	if (pcap_setnonblock(interface->ppcap, 1, szErrbuf) < 0) {
		log_err("Error setting %s to nonblocking mode: %s", name, szErrbuf);
	}

	int nLinkEncap = pcap_datalink(interface->ppcap);

	/* match (RTS BF) or (DATA, DATA SHORT, RTS (and port)) */
	if (nLinkEncap == DLT_IEEE802_11_RADIO) {
		// if (param_rc_protocol != 99) { // only match on R/C packets if R/C enabled
		/*sprintf(szProgram, "ether[0x00:4] == 0xb4bf0000 || ((ether[0x00:2] == 0x0801 ||
	ether[0x00:2] == 0x0802 || ether[0x00:4] == 0xb4010000) && ether[0x04:1] == 0x%.2x)",
	port_encoded);
	} else {*/
		sprintf(szProgram,
			"(ether[0x00:2] == 0x0801 || ether[0x00:2] == 0x0802 || "
			"ether[0x00:4] == 0xb4010000) && ether[0x04:1] == 0x%.2x",
			port_encoded);
		//}
	} else {
		log_err("ERROR: unknown encapsulation on %s! check if monitor mode is supported "
			"and enabled",
			name);
		exit(1);
	}

	if (pcap_compile(interface->ppcap, &bpfprogram, szProgram, 1, 0) == -1) {
		puts(szProgram);
		puts(pcap_geterr(interface->ppcap));
		exit(1);
	} else {
		if (pcap_setfilter(interface->ppcap, &bpfprogram) == -1) {
			log_err("%s", szProgram);
			log_err("%s", pcap_geterr(interface->ppcap));
		} else {
		}

		pcap_freecode(&bpfprogram);
	}

	interface->selectable_fd = pcap_get_selectable_fd(interface->ppcap);
}

int
wfb_rx_packet_interface(monitor_interface_t *interface, wfb_rx_packet_t *rx_data)
{
	int result = 0;

	struct pcap_pkthdr *ppcapPacketHeader = NULL;

	struct ieee80211_radiotap_iterator rti;
	// PENUMBRA_RADIOTAP_DATA prd;
	uint8_t payloadBuffer[300];
	uint8_t *pu8Payload = payloadBuffer;
	ssize_t bytes;
	int n;
	int retval;
	size_t u16HeaderLen;

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

	// fetch radiotap header length from radiotap header (seems to be 36 for Atheros and 18 for
	// Ralink)
	u16HeaderLen = (size_t)(pu8Payload[2] + (pu8Payload[3] << 8));
	//  fprintf(stderr, "u16headerlen: %d\n", u16HeaderLen);

	pu8Payload += u16HeaderLen;
	switch (pu8Payload[1]) {
	case 0xBF:
		/* RTS (R/C) */
		// log_dbg("RTS R/C frame");
		interface->n80211HeaderLength = 0x04;
		rx_data->type = 0;
		break;
	case 0x01:
		/* Data short, RTS telemetry */
		// log_dbg("Data short or RTS telemetry frame");
		interface->n80211HeaderLength = 0x05;
		rx_data->type = 1;
		break;
	case 0x02:
		/* Data telemetry */
		// log_dbg("Data telemetry frame");
		interface->n80211HeaderLength = 0x18;
		rx_data->type = 1;
		break;
	default:
		break;
	}
	pu8Payload -= u16HeaderLen;

	// log_dbg("ppcapPacketHeader->len: %d", ppcapPacketHeader->len);
	if (ppcapPacketHeader->len < (bpf_u_int32)(u16HeaderLen + interface->n80211HeaderLength)) {
		exit(1);
	}

	bytes = (ssize_t)(ppcapPacketHeader->len - (u16HeaderLen + interface->n80211HeaderLength));
	// log_dbg(stderr, "bytes: %d", bytes);
	if (bytes < 0) {
		exit(1);
	}

	if (ieee80211_radiotap_iterator_init(&rti, (struct ieee80211_radiotap_header *)pu8Payload,
					     ppcapPacketHeader->len, &vns) < 0) {
		exit(1);
	}

	int dbm = -127;

	while ((n = ieee80211_radiotap_iterator_next(&rti)) == 0) {
		switch (rti.this_arg_index) {
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
			int8_t signal_dbm = (int8_t)(*rti.this_arg);
			if ((signal_dbm < 0) && (signal_dbm > -126)) {
				if (signal_dbm > dbm) {
					dbm = signal_dbm;
				}
			}
		} break;

		default:
			/* do nothing */
			break;
		}
	}

	rx_data->dbm = dbm;

	if (bytes > MAX_MTU) {
		bytes = MAX_MTU;
	}
	rx_data->bytes = bytes;

	pu8Payload += u16HeaderLen + interface->n80211HeaderLength;
	memcpy(rx_data->data, pu8Payload, (size_t)bytes);

	result = 1;

	return result;
}

int
wfb_rx_packet(wfb_rx_t *wfb_rx, wfb_rx_packet_t *rx_data)
{
	int result = 0;

	struct timeval to;
	to.tv_sec = 0;
	to.tv_usec = 1e5; // 100ms timeout
	fd_set readset;
	FD_ZERO(&readset);
	int nfds = 0;

	size_t i;
	for (i = 0; i < wfb_rx->count; i++) {
		FD_SET(wfb_rx->iface[i].selectable_fd, &readset);
		if (wfb_rx->iface[i].selectable_fd > nfds) {
			nfds = wfb_rx->iface[i].selectable_fd;
		}
	}

	result = select(nfds + 1, &readset, NULL, NULL, &to);

	if (result > 0) {
		for (i = 0; i < wfb_rx->count; i++) {
			if (FD_ISSET(wfb_rx->iface[i].selectable_fd, &readset)) {
				result = wfb_rx_packet_interface(&wfb_rx->iface[i], rx_data);
				rx_data->adapter = i;
				break;
			}
		}
	}

	return result;
}

int
wfb_rx_init(wfb_rx_t *wfb_rx, int port)
{
	int result = 0;

	if_desc_t if_list[4U];
	int if_count;

	if_count = nl_get_wifi_list(if_list);
	if (if_count < 0) {
		result = -1;
		log_err("cannot get wlan list");
		return result;
	}
	size_t num_if = (size_t)if_count;

	char path[128], line[100];
	FILE *procfile;

	wfb_rx->count = 0U;

	size_t i;
	for (i = 0U; i < num_if; i++) {
		snprintf(path, 128, "/sys/class/net/%s/device/uevent", if_list[i].ifname);
		procfile = fopen(path, "r");
		if (!procfile) {
			log_err("opening %s failed!", path);
			result = -1;
			break;
		}

		/* skip first line, need second line */
		size_t l;
		for (l = 0U; l < 2U; l++) {
			if (fgets(line, 100, procfile) == NULL) {
				result = -1;
				break;
			}
		}
		fclose(procfile);

		if (result != 0) {
			break;
		}

		if (strncmp(line, "DRIVER=ath9k_htc", 16) == 0) { // it's an atheros card
			log_inf("RX_RC_TELEMETRY: Driver: Atheros");
			wfb_rx->type[wfb_rx->count] = (int8_t)(0);
		} else { // ralink
			log_inf("RX_RC_TELEMETRY: Driver: Ralink or Realtek");
			wfb_rx->type[wfb_rx->count] = (int8_t)(1);
		}

		open_and_configure_interface(if_list[i].ifname, &wfb_rx->iface[wfb_rx->count],
					     port);
		wfb_rx->count++;

		usleep(10000); // wait a bit between configuring interfaces to reduce Atheros and Pi
			       // USB flakiness
	}

	return result;
}
