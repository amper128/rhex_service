/**
 * @file wfb_tx.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции отправки данных через wifi broadcast
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

#include <log/log.h>
#include <wfb/wfb_tx.h>

/* header buffer for atheros */
static uint8_t headers_atheros[40];
/* header buffer for ralink */
static uint8_t headers_ralink[40];
/* header buffer for Realtek */
static uint8_t headers_Realtek[40];

static int headers_atheros_len = 0;
static int headers_ralink_len = 0;
static int headers_Realtek_len = 0;

/* wifi packet to be sent (263 + len and seq + radiotap and ieee headers) */
static uint8_t packet_buffer_ath[402];
/* wifi packet to be sent (263 + len and seq + radiotap and ieee headers) */
static uint8_t packet_buffer_ral[402];
/* wifi packet to be sent (263 + len and seq + radiotap and ieee headers) */
static uint8_t packet_buffer_rea[402];

/* telemetry frame header consisting of seqnr and payload length */
struct header_s {
	uint32_t seqnumber;
	uint16_t length;
} __attribute__((__packed__));

static struct header_s header;

int
wfb_open_sock(const if_desc_t *iface)
{
	struct sockaddr_ll ll_addr;
	struct ifreq ifr;

	int sock;

	sock = socket(AF_PACKET, SOCK_RAW, 0);
	if (sock == -1) {
		log_err("Socket failed");
		exit(1);
	}

	ll_addr.sll_family = AF_PACKET;
	ll_addr.sll_protocol = 0;
	ll_addr.sll_halen = ETH_ALEN;

	strncpy(ifr.ifr_name, iface->ifname, IFNAMSIZ);
	ifr.ifr_ifindex = iface->ifi_index;
	ll_addr.sll_ifindex = ifr.ifr_ifindex;

	if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
		log_err("ioctl(SIOCGIFHWADDR) failed");
		exit(1);
	}

	memcpy(ll_addr.sll_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	if (bind(sock, (struct sockaddr *)&ll_addr, sizeof(ll_addr)) == -1) {
		log_err("bind failed");
		close(sock);
		exit(1);
	}

	if (sock == -1) {
		log_err("Cannot open socket");
		log_inf("Must be root with an 802.11 card with RFMON enabled");
		exit(1);
	}

	return sock;
}

static uint8_t u8aRadiotapHeader[] = {0x00, 0x00,	      /**< @brief radiotap version */
				      0x0c, 0x00,	      /**< @brief radiotap header length */
				      0x04, 0x80, 0x00, 0x00, /**< @brief radiotap present flags */
				      0x00, /**< @brief datarate (will be overwritten later) */
				      0x00, 0x00, 0x00};

static uint8_t u8aRadiotapHeader80211n[] = {
    0x00, 0x00,		    /**< @brief radiotap version */
    0x0d, 0x00,		    /**< @brief radiotap header length */
    0x00, 0x80, 0x08, 0x00, /**< @brief radiotap present flags (tx flags, mcs) */
    0x08, 0x00,		    /**< @brief tx-flag */
    0x37,		    /**< @brief mcs have: bw, gi, stbc, fec */
    0x30,		    /**< @brief mcs: 20MHz bw, long guard interval, stbc, ldpc */
    0x00,		    /**< @brief mcs index 0 (speed level, will be overwritten later) */
};

static uint8_t u8aIeeeHeader_data[] = {
    0x08, 0x02, 0x00, 0x00, /**< @brief frame control field (2 bytes), duration (2 bytes) */
    0xff, 0x00, 0x00, 0x00,
    0x00, 0x00, /**< @brief 1st byte of MAC will be overwritten with encoded port */
    0x13, 0x22, 0x33, 0x44,
    0x55, 0x66, /**< @brief mac */
    0x13, 0x22, 0x33, 0x44,
    0x55, 0x66, /**< @brief mac */
    0x00, 0x00	/**< @brief IEEE802.11 seqnum, (will be overwritten later by Atheros firmware/wifi
		   chip) */
};

static uint8_t u8aIeeeHeader_data_short[] = {
    0x08, 0x01, 0x00, 0x00, /**< @brief frame control field (2 bytes), duration (2 bytes) */
    0xff		    /**< @brief 1st byte of MAC will be overwritten with encoded port */
};

static uint8_t u8aIeeeHeader_rts[] = {
    0xb4, 0x01, 0x00, 0x00, /**< @brief frame control field (2 bytes), duration (2 bytes) */
    0xff		    /**< @brief 1st byte of MAC will be overwritten with encoded port */
};

static uint8_t dummydata[] = {0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
			      0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd};

int flagHelp = 0;

static void
wfb_write(wfb_tx_t *wfb_tx, uint32_t seqno, const uint8_t data[], uint16_t len, bool add_header)
{
	int padlen = 0;
	size_t i;
	size_t offset = 0U;

	if (add_header) {
		header.seqnumber = seqno;
		header.length = len;
	}

	for (i = 0; i < wfb_tx->count; i++) {
		switch (wfb_tx->type[i]) {
		case 0:
			/* type: Ralink */
			if (add_header) {
				/* header (seqno and len) */
				memcpy(packet_buffer_ral + headers_ralink_len, &header, 6U);
				offset = 6U;
			}
			memcpy(packet_buffer_ral + headers_ralink_len + offset, data, len);

			if (len < 18U) {
				/* pad to minimum length */
				padlen = 18U - len;
				memcpy(packet_buffer_ral + headers_ralink_len + offset + len,
				       dummydata, padlen);
			}

			if (write(wfb_tx->sock[i], &packet_buffer_ral,
				  headers_ralink_len + offset + len + padlen) < 0) {
				log_err("Cannot write sock");
				exit(1);
			}

			break;

		case 1:
			/* type: atheros */
			if (add_header) {
				memcpy(packet_buffer_ath + headers_atheros_len, &header, 6U);
				offset = 6U;
			}
			/* data */
			memcpy(packet_buffer_ath + headers_atheros_len + offset, data, len);
			if (len < 5U) {
				padlen = 5U - len;
				memcpy(packet_buffer_ath + headers_atheros_len + offset + len,
				       dummydata, padlen);
			}

			if (write(wfb_tx->sock[i], &packet_buffer_ath,
				  headers_atheros_len + offset + len + padlen) < 0) {
				log_err("Cannot write sock");
				exit(1);
			}

			break;

		case 2:
			/* type: Realtek */
			if (add_header) {
				memcpy(packet_buffer_rea + headers_Realtek_len, &header, 6U);
				offset = 6U;
			}
			memcpy(packet_buffer_rea + headers_Realtek_len + offset, data, len);
			if (len < 5U) {
				/* if payload is too short, pad to minimum length */
				padlen = 5U - len;
				memcpy(packet_buffer_rea + headers_Realtek_len + offset + len,
				       dummydata, padlen);
			}

			if (write(wfb_tx->sock[i], &packet_buffer_rea,
				  headers_Realtek_len + offset + len + padlen) < 0) {
				log_err("Cannot write sock");
				exit(1);
			}

			break;

		default:
			log_err("Wrong or no frame type specified (see -t parameter)");
			exit(1);

			break;
		}
	}

	wfb_tx->pcnt++;

	if (wfb_tx->pcnt % 128 == 0) {
		log_inf("%d packets sent", wfb_tx->pcnt);
	}
}

void
wfb_tx_send(wfb_tx_t *wfb_tx, uint32_t seqno, const uint8_t data[], uint16_t len)
{
	wfb_write(wfb_tx, seqno, data, len, true);
}

void
wfb_tx_send_raw(wfb_tx_t *wfb_tx, const uint8_t data[], uint16_t len)
{
	wfb_write(wfb_tx, 0U, data, len, false);
}

int
wfb_tx_init(wfb_tx_t *wfb_tx, int port, bool use_cts)
{
	int result = 0;

	do {
		if_desc_t if_list[NL_MAX_IFACES];
		int res;

		res = nl_get_wifi_list(if_list);
		if (res < 0) {
			result = res;
			log_err("cannot get wlan list");
			break;
		}

		size_t num_interfaces = 0U;
		size_t num_if = (size_t)res;

		int port_encoded = 0;
		int param_data_rate = 12;
		size_t i;

		for (i = 0; (i < num_if) && (num_interfaces < NL_MAX_IFACES); i++) {
			FILE *procfile;
			char line[100];
			char path[128];

			snprintf(path, 128, "/sys/class/net/%s/device/uevent", if_list[i].ifname);
			procfile = fopen(path, "r");

			if (!procfile) {
				log_err("opening %s failed!", path);
				return 0;
			}

			size_t l;

			/* skip first line, need second line */
			for (l = 0U; l < 2U; l++) {
				if (fgets(line, 100, procfile) == NULL) {
					result = -1;
					break;
				}
			}
			fclose(procfile);

			if (strncmp(line, "DRIVER=ath9k_htc", 16) == 0 ||
			    (strncmp(line, "DRIVER=8812au", 13) == 0 ||
			     strncmp(line, "DRIVER=8814au", 13) == 0 ||
			     strncmp(line, "DRIVER=rtl8812au", 16) == 0 ||
			     strncmp(line, "DRIVER=rtl8814au", 16) == 0 ||
			     strncmp(line, "DRIVER=rtl88xxau", 16) == 0)) {
				if (strncmp(line, "DRIVER=ath9k_htc", 16) == 0) {
					log_inf("tx_telemetry: Atheros card detected");
					wfb_tx->type[num_interfaces] = 1;
				} else {
					log_inf("tx_telemetry: Realtek card detected");
					wfb_tx->type[num_interfaces] = 2;
				}
			} else { // ralink or mediatek
				log_inf("tx_telemetry: Ralink or other type card detected");
				wfb_tx->type[num_interfaces] = 0;
			}

			wfb_tx->sock[wfb_tx->count] = wfb_open_sock(&if_list[i]);
			if (wfb_tx->sock[wfb_tx->count] > 0) {
				wfb_tx->count++;

				usleep(10000); // wait a bit between configuring interfaces to
					       // reduce Atheros and Pi USB flakiness
			}
		}

		if (result != 0) {
			return result;
		}

		switch (param_data_rate) {
		case 1:
			u8aRadiotapHeader[8] = 0x02;
			break;
		case 2:
			u8aRadiotapHeader[8] = 0x04;
			break;
		case 5: // 5.5
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
			log_err("tx_telemetry: ERROR: Wrong or no data rate specified (see -d "
				"parameter)");
			exit(1);
			break;
		}

		port_encoded = (port * 2) + 1;
		u8aIeeeHeader_rts[4] = port_encoded;
		u8aIeeeHeader_data[4] = port_encoded;
		u8aIeeeHeader_data_short[4] = port_encoded;

		// for Atheros use data frames if CTS protection enabled or rts if disabled
		// CTS protection causes R/C transmission to stop for some reason, always use rts
		// frames (i.e. no cts protection) use_cts = 0;
		if (use_cts) { // use data frames
			memcpy(headers_atheros, u8aRadiotapHeader,
			       sizeof(u8aRadiotapHeader)); // radiotap header
			memcpy(headers_atheros + sizeof(u8aRadiotapHeader), u8aIeeeHeader_data,
			       sizeof(u8aIeeeHeader_data)); // ieee header
			headers_atheros_len =
			    sizeof(u8aRadiotapHeader) + sizeof(u8aIeeeHeader_data);
		} else { // use rts frames
			memcpy(headers_atheros, u8aRadiotapHeader,
			       sizeof(u8aRadiotapHeader)); // radiotap header
			memcpy(headers_atheros + sizeof(u8aRadiotapHeader), u8aIeeeHeader_rts,
			       sizeof(u8aIeeeHeader_rts)); // ieee header
			headers_atheros_len = sizeof(u8aRadiotapHeader) + sizeof(u8aIeeeHeader_rts);
		}

		// for Ralink always use data short
		memcpy(headers_ralink, u8aRadiotapHeader,
		       sizeof(u8aRadiotapHeader)); // radiotap header
		memcpy(headers_ralink + sizeof(u8aRadiotapHeader), u8aIeeeHeader_data_short,
		       sizeof(u8aIeeeHeader_data_short)); // ieee header
		headers_ralink_len = sizeof(u8aRadiotapHeader) + sizeof(u8aIeeeHeader_data_short);

		// for Realtek use rts frames
		memcpy(headers_Realtek, u8aRadiotapHeader80211n,
		       sizeof(u8aRadiotapHeader80211n)); // radiotap header
		memcpy(headers_Realtek + sizeof(u8aRadiotapHeader80211n), u8aIeeeHeader_rts,
		       sizeof(u8aIeeeHeader_rts)); // ieee header
		headers_Realtek_len = sizeof(u8aRadiotapHeader80211n) + sizeof(u8aIeeeHeader_rts);

		// radiotap and ieee headers
		memcpy(packet_buffer_ath, headers_atheros, headers_atheros_len);
		memcpy(packet_buffer_ral, headers_ralink, headers_ralink_len);
		memcpy(packet_buffer_rea, headers_Realtek, headers_Realtek_len);
	} while (false);

	return result;
}
