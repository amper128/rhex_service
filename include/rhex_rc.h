/**
 * @file rc.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Команды управления
 */

#pragma once

#include <stdint.h>

typedef struct {
	float speed;
	float steering;
} rc_data_t;

typedef struct {
	uint64_t last_update;
	uint32_t received_packet_cnt;
	uint32_t lost_packet_cnt;
	int8_t signal_dbm;
} rc_status_t;

int rc_init(void);

int rc_main(void);
