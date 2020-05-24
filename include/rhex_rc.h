/**
 * @file rc.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Команды управления
 */

#pragma once

typedef struct {
	float speed;
	float steering;
} rc_data_t;

int rc_init(void);

int rc_main(void);
