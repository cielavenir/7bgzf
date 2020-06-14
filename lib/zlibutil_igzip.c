#include "isa-l/include/igzip_lib.h"
#include <stdlib.h> // malloc/free

int igzip_level_size_buf[10] = {
#ifdef ISAL_DEF_LVL0_DEFAULT
	ISAL_DEF_LVL0_DEFAULT,
#else
	0,
#endif
#ifdef ISAL_DEF_LVL1_DEFAULT
	ISAL_DEF_LVL1_DEFAULT,
#else
	0,
#endif
#ifdef ISAL_DEF_LVL2_DEFAULT
	ISAL_DEF_LVL2_DEFAULT,
#else
	0,
#endif
#ifdef ISAL_DEF_LVL3_DEFAULT
	ISAL_DEF_LVL3_DEFAULT,
#else
	0,
#endif
#ifdef ISAL_DEF_LVL4_DEFAULT
	ISAL_DEF_LVL4_DEFAULT,
#else
	0,
#endif
#ifdef ISAL_DEF_LVL5_DEFAULT
	ISAL_DEF_LVL5_DEFAULT,
#else
	0,
#endif
#ifdef ISAL_DEF_LVL6_DEFAULT
	ISAL_DEF_LVL6_DEFAULT,
#else
	0,
#endif
#ifdef ISAL_DEF_LVL7_DEFAULT
	ISAL_DEF_LVL7_DEFAULT,
#else
	0,
#endif
#ifdef ISAL_DEF_LVL8_DEFAULT
	ISAL_DEF_LVL8_DEFAULT,
#else
	0,
#endif
#ifdef ISAL_DEF_LVL9_DEFAULT
	ISAL_DEF_LVL9_DEFAULT,
#else
	0,
#endif
};

int igzip_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
#if defined(NOIGZIP)
	return -1;
#else
	struct isal_zstream z;
	isal_deflate_stateless_init(&z);

	z.next_in = (unsigned char*)source;
	z.avail_in = sourceLen;
	z.next_out = dest;
	z.avail_out = *destLen;
	z.flush = FULL_FLUSH;
	z.gzip_flag = 0;
	z.level = level-1;
	z.level_buf_size = igzip_level_size_buf[z.level];
	z.level_buf = malloc(z.level_buf_size);
	z.end_of_stream = 1;

	int status = isal_deflate_stateless(&z);
	free(z.level_buf);
	if(status != COMP_OK){
		//fprintf(stderr,"deflate: %s\n", (z.msg) ? z.msg : "???");
		return status;
	}
	*destLen-=z.avail_out;

	return 0;
#endif
}

int igzip_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
){
#if defined(NOIGZIP)
	return -1;
#else
	struct inflate_state z;
	isal_inflate_init(&z);

	z.next_in = (unsigned char*)source;
	z.avail_in = sourceLen;
	z.next_out = dest;
	z.avail_out = *destLen;

	int status = isal_inflate_stateless(&z);
	if(status != ISAL_DECOMP_OK && status != ISAL_END_INPUT){
		//fprintf(stderr,"deflate: %s\n", (z.msg) ? z.msg : "???");
		return status;
	}
	*destLen-=z.avail_out;

	return 0;
#endif
}
