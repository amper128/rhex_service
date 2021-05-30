/**
 * @file wfb_tx.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции отправки данных через wifi broadcast
 */

#pragma once

#include <netlink.h>
#include <platform.h>

#define MAX_ADAP (5U)

typedef struct {
	int sock[MAX_ADAP];
	int type[MAX_ADAP];
	size_t count;
	size_t pcnt;
	size_t stream_phdr_len;
} wfb_tx_t;

int wfb_open_sock(const char ifname[]);

int wfb_tx_init(wfb_tx_t *wfb_tx, size_t num_if, const if_desc_t interfaces[], int port);

void wfb_tx_send(wfb_tx_t *wfb_tx, uint32_t seqno, uint8_t data[], uint16_t len);
