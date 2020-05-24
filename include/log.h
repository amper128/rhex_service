/**
 * @file log.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции журналирования
 */

#pragma once

#include <stdarg.h>

void log_dbg(char* format, ...);

void log_inf(char* format, ...);

void log_warn(char* format, ...);

void log_err(char* format, ...);

void log_exc(char* format, ...);
