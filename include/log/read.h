/**
 * @file read.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Чтение журналов
 */

#pragma once

#include <log/log.h>

void log_print(const char name[], log_buffer_t *log);
