/**
 * @file wfb_status.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Описание структур данных состояния WFB
 */

#pragma once

#include <stdint.h>
#include <time.h>

#define MAX_PENUMBRA_INTERFACES 8

typedef struct {
	uint32_t received_packet_cnt;
	uint32_t wrong_crc_cnt;
	int8_t current_signal_dbm;
	/* 0 = Atheros / 1 = Ralink */
	int8_t type;
	int signal_good;
} wifi_adapter_rx_status_t;

typedef struct {
	time_t last_update;
	uint32_t received_block_cnt;
	uint32_t damaged_block_cnt;
	uint32_t lost_packet_cnt;
	uint32_t received_packet_cnt;
	uint32_t lost_per_block_cnt;
	uint32_t tx_restart_cnt;
	uint32_t kbitrate;
	uint32_t current_air_datarate_kbit;
	uint32_t wifi_adapter_cnt;
	wifi_adapter_rx_status_t adapter[MAX_PENUMBRA_INTERFACES];
} wifibroadcast_rx_status_t;

typedef struct {
	time_t last_update;
	uint32_t injected_block_cnt;
	uint32_t skipped_fec_cnt;
	uint32_t injection_fail_cnt;
	long long injection_time_block;
} wifibroadcast_tx_status_t;

typedef struct {
	time_t last_update;
	uint32_t received_block_cnt;
	uint32_t damaged_block_cnt;
	uint32_t lost_packet_cnt;
	uint32_t received_packet_cnt;
	uint32_t lost_per_block_cnt;
	uint32_t tx_restart_cnt;
	uint32_t kbitrate;
	uint32_t wifi_adapter_cnt;
	wifi_adapter_rx_status_t adapter[MAX_PENUMBRA_INTERFACES];
} wifibroadcast_rx_status_t_rc;
