/**
 * @file svc_context.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Контекст микросервиса
 */

#pragma once

#include <log/log.h>
#include <svc/platform.h>

typedef struct {
	uint64_t period;
	uint64_t watchdog;
	int timerfd;
	log_buffer_t *log_buffer;
} svc_context_t;

const svc_context_t *get_svc_context(void);

svc_context_t *svc_create_context(const char svc_name[]);

void svc_init_context(svc_context_t *ctx);

bool svc_cycle(void);

uint64_t svc_get_monotime(void);

uint64_t svc_get_time(void);
