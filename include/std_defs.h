/**
 * @file platform.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Определения стандартных типов
 */

#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef u32 __le32;
typedef unsigned long ulong;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#	define	le16_to_cpu(x) (x)
#	define	le32_to_cpu(x) (x)
#else
#	define	le16_to_cpu(x) ((((x)&0xff)<<8)|(((x)&0xff00)>>8))
#	define	le32_to_cpu(x) \
		((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)&0xff0000)>>8)|(((x)&0xff000000)>>24))
#endif

#define	unlikely(x) (x)
