/**
 * @file crc.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции подсчета CRC
 */

#include <private/crc.h>

static uint16_t
vt_crc16worker(uint16_t icrc, uint8_t r0)
{
	union {
		uint16_t crc16; // 16-bit CRC
		struct {
			uint8_t crcl, crch;
		} s;
	} u;

	uint8_t a1; // scratch byte

	u.crc16 = icrc;

	r0 = r0 ^ u.s.crch;
	a1 = u.s.crcl;
	u.s.crch = r0;
	u.s.crch = (u.s.crch << 4) | (u.s.crch >> 4);
	u.s.crcl = u.s.crch ^ r0;
	u.crc16 &= 0x0ff0;
	r0 ^= u.s.crch;
	a1 ^= u.s.crcl;
	u.crc16 <<= 1;
	u.s.crcl ^= r0;
	u.s.crch ^= a1;

	return (u.crc16);
}

uint16_t
vt_crc16(uint8_t data[], size_t size, uint16_t seed)
{
	uint16_t i;
	uint16_t crc;

	crc = seed;

	for (i = 0; i < size; i++) {
		crc = vt_crc16worker(crc, data[i]);
	}

	return crc;
}
