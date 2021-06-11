/**
 * @file timerfd.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции работы с таймером
 */

#pragma once

#include <svc/platform.h>

int timerfd_init(uint64_t start_nsec, uint64_t period_nsec);

bool timerfd_wait(int fd);
