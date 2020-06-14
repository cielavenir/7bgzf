#ifndef _ZLIBUTIL_H_
#define _ZLIBUTIL_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include "zlib/zlib.h"

int read_gz_header_generic(unsigned char *data, int size, int *extra_off, int *extra_len);

enum{
	DEFLATE_ZLIB = 0,
	DEFLATE_7ZIP,
	DEFLATE_ZOPFLI,
	DEFLATE_MINIZ,
	DEFLATE_SLZ,
	DEFLATE_LIBDEFLATE,
	DEFLATE_ZLIBNG,
	DEFLATE_IGZIP,
	DEFLATE_CRYPTOPP,
};

typedef struct{
	unsigned char *dest;
	size_t destLen;
	unsigned char *source;
	size_t sourceLen;
	void *func;
	int encode;
	int level;
	int rfc1950;
	int ret;
} zlibutil_buffer;
zlibutil_buffer *zlibutil_buffer_allocate(size_t destSiz, size_t sourceSiz);
zlibutil_buffer *zlibutil_buffer_code(zlibutil_buffer *zlibbuf);
void zlibutil_buffer_free(zlibutil_buffer* zlibbuf);

typedef int (*zlibutil_code_dec)(unsigned char*,size_t*,unsigned char*,size_t);
typedef int (*zlibutil_code_enc)(unsigned char*,size_t*,unsigned char*,size_t,int);

int zlibutil_auto_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
);

// need lzmaOpen7z() in prior.
// I'm not sure how much it costs to create coder each time.
// to use persistent coder, do:
// lzmaCreateCoder(&coder,0x040108,1,level); lzmaCodeOneshot(coder,decbuf,decsize,compbuf,&compsize);
int lzma_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

// lzmaCreateCoder(&coder,0x040108,0,level); lzmaCodeOneshot(coder,compbuf,compsize,decbuf,&decsize);
int lzma_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
);

int miniz_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int miniz_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
);

void slz_initialize();

int slz_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int libdeflate_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int libdeflate_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
);

int zopfli_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int zlib_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int zlib_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
);

int zlibng_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int zlibng_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
);

int igzip_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int igzip_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
);

int cryptopp_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int cryptopp_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
);

#ifdef __cplusplus
}
#endif

#endif
