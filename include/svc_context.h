/**
 * @file svc_context.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Контекст микросервиса
 */

#pragma once

#include <logger.h>
#include <platform.h>

typedef struct {
	int timerfd;
	log_buffer_t *log_buffer;
	uint64_t watchdog;
} svc_context_t;

const svc_context_t *get_svc_context(void);

svc_context_t *svc_create_context(const char svc_name[]);

void svc_init_context(svc_context_t *ctx);

bool svc_cycle(void);

uint64_t svc_get_time(void);
