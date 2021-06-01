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

typedef struct {
	int sock[NL_MAX_IFACES];
	int type[NL_MAX_IFACES];
	size_t count;
	size_t pcnt;
	size_t stream_phdr_len;
} wfb_tx_t;

int wfb_open_sock(const char ifname[]);

int wfb_tx_init(wfb_tx_t *wfb_tx, int port, bool use_cts);

void wfb_tx_send(wfb_tx_t *wfb_tx, uint32_t seqno, const uint8_t data[], uint16_t len);

void wfb_tx_send_raw(wfb_tx_t *wfb_tx, const uint8_t data[], uint16_t len);
