/**
 * @file wfb_rx.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции приема данных wifi broadcast
 */

#pragma once

#include <pcap.h>
#include <svc/netlink.h>
#include <svc/platform.h>

#define MAX_MTU (1500)

typedef struct {
	pcap_t *ppcap;
	int selectable_fd;
	int n80211HeaderLength;
} monitor_interface_t;

typedef struct {
	monitor_interface_t iface[NL_MAX_IFACES];
	int8_t type[NL_MAX_IFACES];
	size_t count;
} wfb_rx_t;

typedef struct {
	size_t adapter;
	int type;  // r/c or telemetry
	int dbm;   // signal level
	int bytes; // data length
	uint8_t data[MAX_MTU];
} wfb_rx_packet_t;

int wfb_rx_init(wfb_rx_t *wfb_rx, int port);

int wfb_rx_packet(wfb_rx_t *wfb_rx, wfb_rx_packet_t *rx_data);

int wfb_rx_packet_interface(monitor_interface_t *interface, wfb_rx_packet_t *rx_data);
