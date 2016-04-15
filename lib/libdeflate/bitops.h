/*
 * bitops.h - inline functions for bit manipulation
 */

#pragma once

#include "util.h"

/* Find Last Set bit   */

static forceinline unsigned
fls32(u32 v)
{
#ifdef compiler_fls32
	return compiler_fls32(v);
#else
	unsigned bit = 0;
	while ((v >>= 1) != 0)
		bit++;
	return bit;
#endif
}

static forceinline unsigned
fls64(u64 v)
{
#ifdef compiler_fls64
	return compiler_fls64(v);
#else
	unsigned bit = 0;
	while ((v >>= 1) != 0)
		bit++;
	return bit;
#endif
}

static forceinline unsigned
flsw(machine_word_t v)
{
	STATIC_ASSERT(WORDSIZE == 4 || WORDSIZE == 8);
	if (WORDSIZE == 4)
		return fls32(v);
	else
		return fls64(v);
}

/* Find First Set bit   */

static forceinline unsigned
ffs32(u32 v)
{
#ifdef compiler_ffs32
	return compiler_ffs32(v);
#else
	unsigned bit;
	for (bit = 0; !(v & 1); bit++, v >>= 1)
		;
	return bit;
#endif
}

static forceinline unsigned
ffs64(u64 v)
{
#ifdef compiler_ffs64
	return compiler_ffs64(v);
#else
	unsigned bit;
	for (bit = 0; !(v & 1); bit++, v >>= 1)
		;
	return bit;
#endif
}

static forceinline unsigned
ffsw(machine_word_t v)
{
	STATIC_ASSERT(WORDSIZE == 4 || WORDSIZE == 8);
	if (WORDSIZE == 4)
		return ffs32(v);
	else
		return ffs64(v);
}
