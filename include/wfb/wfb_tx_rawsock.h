/**
 * @file wfb_tx_rawsock.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Функции отправки потока через wifi broadcast
 */

#pragma once

#include <wfb/wfb_tx.h>

typedef struct {
	int valid;
	int crc_correct;
	size_t len; /* actual length of the packet stored in data */
	uint8_t *data;
} packet_buffer_t;

typedef struct {
	int seq_nr;
	size_t curr_pb;
	packet_buffer_t *pbl;
} input_buffer_t;

typedef struct {
	wfb_tx_t wfb_tx;
	size_t stream_phdr_len;
	input_buffer_t input_buffer;
	int port;
} wfb_tx_rawsock_t;
