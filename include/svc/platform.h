/**
 * @file platform.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Основные определения
 */

#pragma once

#include <endian.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef u32 __le32;
typedef unsigned long ulong;

#define unlikely(x) (x)

#define le16_to_cpu le16toh
#define le32_to_cpu le32toh
#define get_unaligned(p)                                                                           \
	({                                                                                         \
		struct packed_dummy_struct {                                                       \
			typeof(*(p)) __val;                                                        \
		} __attribute__((packed)) *__ptr = (void *)(p);                                    \
												   \
		__ptr->__val;                                                                      \
	})

#define get_unaligned_le16(p) le16_to_cpu(get_unaligned((uint16_t *)(p)))
#define get_unaligned_le32(p) le32_to_cpu(get_unaligned((uint32_t *)(p)))

#define TIME_NS (1ULL)
#define TIME_US (1000ULL)
#define TIME_MS (1000000ULL)
#define TIME_S (1000000000ULL)
