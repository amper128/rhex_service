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
#include <stddef.h>
#include <stdbool.h>

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
