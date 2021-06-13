/**
 * @file log.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020-2021
 * @brief Функции журналирования
 */

#pragma once

#include <stdarg.h>
#include <svc/platform.h>

#define LOG_ITEM_MAXMSG (32U)

enum log_level {
	LOG_DBG = 0, /**< @brief Уровень отладки */
	LOG_INF,     /**< @brief Уровень информации */
	LOG_WARN,    /**< @brief Уровень предупреждений */
	LOG_ERR,     /**< @brief Уровень ошибок */
	LOG_EXC	     /**< @brief Уровень исключений */
};

struct log_record_t {
	enum log_level level;
	uint64_t date;
	uint8_t msg_len;
	uint8_t log_continue;
	uint8_t __pad[2U];
	char msg[LOG_ITEM_MAXMSG];
};

#define LOG_BUFFER_SIZE (64U)

typedef struct {
	uint32_t head;
	uint32_t tail;
	struct log_record_t records[LOG_BUFFER_SIZE];
} log_buffer_t;

log_buffer_t *log_create(const char name[]);

void log_init(void);

void log_dbg(const char *format, ...);

void log_inf(const char *format, ...);

void log_warn(const char *format, ...);

void log_err(const char *format, ...);

void log_exc(const char *format, ...);
