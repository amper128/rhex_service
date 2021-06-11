/**
 * @file log/record.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Формат записи в лог
 */

#pragma once

#include <log/log.h>

void log_put_record(enum log_level level, const char format[], va_list args);

log_buffer_t *get_log_buffer(void);
