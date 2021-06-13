/**
 * @file wfb_tx_rawsock.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Функции отправки потока через wifi broadcast
 */

#pragma once

#include <wfb/wfb_tx.h>

#define MAX_PACKET_LENGTH (4192)

typedef struct {
	int valid;
	int crc_correct;
	size_t len; /* actual length of the packet stored in data */
	uint8_t *data;
} packet_buffer_t;

typedef struct {
	uint32_t seq_nr;
	size_t curr_pb;
	packet_buffer_t *pbl;
} input_buffer_t;

typedef struct {
	wfb_tx_t wfb_tx;
	size_t phdr_len;
	input_buffer_t input_buffer;
	int port;
	uint8_t buf[MAX_PACKET_LENGTH];
} wfb_stream_t;

int wfb_stream_init(wfb_stream_t *wfb_stream, int port, int packet_type, bool useMCS, bool useSTBC,
		    bool useLDPC);

void wfb_tx_stream(wfb_stream_t *wfb_stream, uint8_t data[], uint16_t len);
