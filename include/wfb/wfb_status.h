/**
 * @file wfb_status.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Описание структур данных состояния WFB
 */

#pragma once

#include <netlink/netlink.h>
#include <stdint.h>
#include <time.h>

typedef struct {
	uint32_t received_packet_cnt;
	uint32_t wrong_crc_cnt;
	int8_t current_signal_dbm;
	/* 0 = Atheros / 1 = Ralink */
	int8_t type;
	int signal_good;
} wifi_adapter_rx_status_t;

typedef struct {
	uint64_t last_update;
	uint32_t received_block_cnt;
	uint32_t damaged_block_cnt;
	uint32_t lost_packet_cnt;
	uint32_t received_packet_cnt;
	uint32_t lost_per_block_cnt;
	uint32_t tx_restart_cnt;
	uint32_t kbitrate;
	uint32_t current_air_datarate_kbit;
	uint32_t wifi_adapter_cnt;
	wifi_adapter_rx_status_t adapter[NL_MAX_IFACES];
} wifibroadcast_rx_status_t;

typedef struct {
	uint64_t last_update;
	uint32_t injected_block_cnt;
	uint32_t skipped_fec_cnt;
	uint32_t injection_fail_cnt;
	uint64_t injection_time_block;
} wifibroadcast_tx_status_t;

typedef struct {
	uint64_t last_update;
	uint32_t received_block_cnt;
	uint32_t damaged_block_cnt;
	uint32_t lost_packet_cnt;
	uint32_t received_packet_cnt;
	uint32_t lost_per_block_cnt;
	uint32_t tx_restart_cnt;
	uint32_t kbitrate;
	uint32_t wifi_adapter_cnt;
	wifi_adapter_rx_status_t adapter[NL_MAX_IFACES];
} wifibroadcast_rx_status_t_rc;

typedef struct {
	uint64_t last_update;
	uint8_t cpuload;
	uint8_t temp;
	uint32_t injected_block_cnt;
	uint32_t skipped_fec_cnt;
	uint32_t injection_fail_cnt;
	uint64_t injection_time_block;
	uint16_t bitrate_kbit;
	uint16_t bitrate_measured_kbit;
	uint8_t cts;
	uint8_t undervolt;
} wifibroadcast_rx_status_t_sysair;

typedef struct {
	uint32_t received_packet_cnt;

	int8_t current_signal_dbm;

	/*
	 * 0 = Atheros,
	 *
	 * 1 = Ralink
	 *
	 */
	int8_t type;

	int8_t signal_good;
} __attribute__((packed)) wifi_adapter_rx_status_forward_t;

typedef struct {
	uint32_t damaged_block_cnt;   // number bad blocks video downstream
	uint32_t lost_packet_cnt;     // lost packets video downstream
	uint32_t skipped_packet_cnt;  // skipped packets video downstream
	uint32_t injection_fail_cnt;  // Video injection failed downstream
	uint32_t received_packet_cnt; // packets received video downstream
	uint32_t kbitrate;	      // live video kilobitrate per second video downstream
	uint32_t kbitrate_measured;   // max measured kbitrate during tx startup
	uint32_t kbitrate_set; // set kilobitrate (measured * bitrate_percent) during tx startup
	uint32_t lost_packet_cnt_telemetry_up;
	uint32_t lost_packet_cnt_telemetry_down;
	uint32_t lost_packet_cnt_msp_up;   // not used at the moment
	uint32_t lost_packet_cnt_msp_down; // not used at the moment
	uint32_t lost_packet_cnt_rc;
	int8_t current_signal_joystick_uplink; // signal strength in dbm at air pi (telemetry
					       // upstream and rc link)
	int8_t current_signal_telemetry_uplink;
	int8_t joystick_connected; // 0 = no joystick connected, 1 = joystick connected
	float HomeLat;
	float HomeLon;
	uint8_t cpuload_gnd;
	uint8_t temp_gnd;
	uint8_t cpuload_air;
	uint8_t temp_air;
	uint8_t vbat_capacity;
	uint8_t is_charging;
	uint16_t vbat_gnd_mv;
	uint32_t wifi_adapter_cnt;
	wifi_adapter_rx_status_forward_t adapter[6]; // same struct as in wifibroadcast lib.h
} __attribute__((packed)) wifibroadcast_rx_status_forward_t;
