/**
 * @file log.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции работы с общей памятью
 */

#pragma once

#include <svc/platform.h>

typedef struct {
	uint64_t guard;
	void *map;
	size_t size;
} shm_t;

bool shm_map_init(const char name[], size_t size);

bool shm_map_open(const char name[], shm_t *shm);

int32_t shm_map_read(shm_t *shm, void **data);

int32_t shm_map_write(shm_t *shm, void *data, size_t size);
