/**
 * @file wfb_tx_rawsock.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Функции отправки потока через wifi broadcast
 */

#pragma once

#include <svc/sharedmem.h>
#include <wfb/wfb_rx.h>
#include <wfb/wfb_status.h>

#define MAX_PACKET_LENGTH (4192)

typedef struct {
	size_t len; /* actual length of the packet stored in data */
	bool valid;
	bool crc_correct;
	uint8_t *data;
} packet_buffer_t;

typedef struct {
	int block_num;
	packet_buffer_t *packet_buffer_list;
} block_buffer_t;

typedef struct {
	wfb_rx_t wfb_rx;
	block_buffer_t *block_buffer_list;
	uint64_t packetcounter_ts_prev[NL_MAX_IFACES];
	uint64_t packetcounter_ts_now[NL_MAX_IFACES];
	size_t packetcounter[NL_MAX_IFACES];
	size_t packetcounter_last[NL_MAX_IFACES];
	size_t bytes_received;
	size_t bytes_decoded;
	uint64_t current_air_datarate_ts;
	shm_t status_shm;
	wifibroadcast_rx_status_t rx_status;
} wfb_rx_stream_t;

typedef struct {
	int bytes; // data length
	uint8_t data[MAX_PACKET_LENGTH * 2U];
} wfb_rx_stream_packet_t;

int wfb_rx_stream_init(wfb_rx_stream_t *rx, int port);

int wfb_rx_stream(wfb_rx_stream_t *rx, wfb_rx_stream_packet_t *rx_data);
