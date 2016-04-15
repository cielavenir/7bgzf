#ifndef _ZLIBUTIL_H_
#define _ZLIBUTIL_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include "zlib/zlib.h"

enum{
	DEFLATE_ZLIB,
	DEFLATE_7ZIP,
	DEFLATE_ZOPFLI,
	DEFLATE_MINIZ,
	DEFLATE_SLZ,
	DEFLATE_LIBDEFLATE,
};

int miniz_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int slz_deflate(
	unsigned char *dest,
	unsigned long *destLen,
	const unsigned char *source,
	unsigned long sourceLen,
	int level
);

int libdeflate_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int zopfli_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int ZEXPORT zlib_deflate(
	unsigned char *dest,
	unsigned long *destLen,
	const unsigned char *source,
	unsigned long sourceLen,
	int level
);

int ZEXPORT zlib_inflate(
	unsigned char *dest,
	unsigned long *destLen,
	const unsigned char *source,
	unsigned long sourceLen
);

#ifdef __cplusplus
}
#endif

#endif
