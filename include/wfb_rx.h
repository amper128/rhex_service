/**
 * @file wfb_rx.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции приема данных wifi broadcast
 */

#pragma once

#include <stdint.h>
#include <pcap.h>

#define INTERFACES_MAX (8U)

typedef struct {
	pcap_t *ppcap;
	int selectable_fd;
	int n80211HeaderLength;
} monitor_interface_t;

typedef struct {
	monitor_interface_t iface[INTERFACES_MAX];
	int8_t type[INTERFACES_MAX];
	size_t count;
} wfb_rx_t;

int wfb_rx_init(wfb_rx_t *wfb_rx, size_t num_if, const char *interfaces[], int port);

#define MAX_MTU (1500)

typedef struct {
	int type;	// r/c or telemetry
	int dbm;	// signal level
	int bytes;	// data length
	uint8_t data[MAX_MTU];
} wfb_rx_packet_t;

int wfb_rx_packet(monitor_interface_t *interface, wfb_rx_packet_t *rx_data);
