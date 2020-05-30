/**
 * @file timerfd.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции работы с таймером
 */

#pragma once

#include <platform.h>

#define TIME_NS (1ULL)
#define TIME_US (1000ULL)
#define TIME_MS (1000000ULL)
#define TIME_S (1000000000ULL)

int timerfd_init(uint64_t start_nsec, uint64_t period_nsec);

bool wait_cycle(int fd);
