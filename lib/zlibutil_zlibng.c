#include "zlib-ng/zlib-ng.h"
#include "zlib-ng/zutil.h" // DEF_MEM_LEVEL

int zlibng_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
#ifdef FEOS
	return -1;
#else
	zng_stream z;
	int status;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if((status=zng_deflateInit2(
		&z, level , Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY
	)) != Z_OK){
		return status;
	}

	z.next_in = source;
	z.avail_in = sourceLen;
	z.next_out = dest;
	z.avail_out = *destLen;

	status = zng_deflate(&z, Z_FINISH); //Z_FULL_FLUSH
	if(status != Z_STREAM_END && status != Z_OK){
		//fprintf(stderr,"deflate: %s\n", (z.msg) ? z.msg : "???");
		return status;
	}
	*destLen-=z.avail_out;

	return zng_deflateEnd(&z);
#endif
}

int zlibng_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
){
#ifdef FEOS
	return -1;
#else
	zng_stream z;
	int status;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if((status=zng_inflateInit2(
		&z, -MAX_WBITS
	)) != Z_OK){
		return status;
	}

	z.next_in = source;
	z.avail_in = sourceLen;
	z.next_out = dest;
	z.avail_out = *destLen;

	for(;z.avail_out && status != Z_STREAM_END;){
		status = zng_inflate(&z, Z_BLOCK);
		if(status==Z_BUF_ERROR)break;
		if(status != Z_STREAM_END && status != Z_OK){
			//fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
			return status;
		}
	}
	*destLen-=z.avail_out;

	return zng_inflateEnd(&z);
#endif
}
