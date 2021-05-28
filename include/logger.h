/**
 * @file logger.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Работа с журналами
 */

#pragma once

#include <stdarg.h>
#include <stdint.h>

enum log_level {
	LOG_DBG = 0, /**< @brief Уровень отладки */
	LOG_INF,     /**< @brief Уровень информации */
	LOG_WARN,    /**< @brief Уровень предупреждений */
	LOG_ERR,     /**< @brief Уровень ошибок */
	LOG_EXC	     /**< @brief Уровень исключений */
};

#define LOG_ITEM_END (0U)
#define LOG_ITEM_CONTINUE (1U)

#define LOG_ITEM_MAXMSG (32U)

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

log_buffer_t *logger_create(const char name[]);

void logger_init(void);

void log_put_record(enum log_level level, const char format[], va_list args);

log_buffer_t *get_log_reader(const char name[]);

void log_reader_print(const char name[], log_buffer_t *log);
