/**
 * @file print.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Печать записей лога
 */

#pragma once

#include <log/log.h>

void msg_print(enum log_level level, const char format[], va_list args);
