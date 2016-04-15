//windowBits: 8..15: zlib, 24..31: gzip, -8..-15: raw

#include <stdbool.h>
#include "zlibutil.h"
#include "miniz.h"
#include "slz.h"
#include "libdeflate/libdeflate.h"
#include "zopfli/deflate.h"

int miniz_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
	tdefl_compressor comp;
	mz_uint comp_flags = tdefl_create_comp_flags_from_zip_params(level, -MAX_WBITS, MZ_DEFAULT_STRATEGY);
	int status=tdefl_init(&comp,NULL,NULL,comp_flags);
	size_t _sourceLen=sourceLen;
	if(status)return status;
	status=tdefl_compress(&comp,source,&_sourceLen,dest,destLen,MZ_FINISH);
	return status==MZ_STREAM_END ? MZ_OK : status;
}

static bool slz_initialized=false;
int slz_deflate(
	unsigned char *dest,
	unsigned long *destLen,
	const unsigned char *source,
	unsigned long sourceLen,
	int level
){
	if(!slz_initialized){
		slz_prepare_dist_table();
		slz_initialized=true;
	}
	struct slz_stream strm;
	slz_init(&strm, level, SLZ_FMT_DEFLATE);
	*destLen=slz_encode(&strm,dest,source,sourceLen,0);
	*destLen+=slz_finish(&strm,dest+*destLen);
	return Z_OK;
}

int libdeflate_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
	struct deflate_compressor *comp=deflate_alloc_compressor(level);
	size_t siz=deflate_compress(comp,source,sourceLen,dest,*destLen);
	deflate_free_compressor(comp);
	if(!siz)return !Z_OK;
	*destLen=siz;
	return Z_OK;
}

int zopfli_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
#ifdef FEOS
	return -1;
#else
	ZopfliOptions options;
	ZopfliInitOptions(&options);
	options.numiterations=level;
	size_t __compsize=0;
	unsigned char bp = 0;
	unsigned char *COMPBUF=NULL;
	ZopfliDeflate(&options, 2 /* Dynamic block */, 1, source, sourceLen, &bp, &COMPBUF, &__compsize);
	if(__compsize<=*destLen){
		memcpy(dest,COMPBUF,__compsize);
		*destLen=__compsize;
	}
	free(COMPBUF);
	return __compsize>*destLen;
#endif
}

int ZEXPORT zlib_deflate(
	unsigned char *dest,
	unsigned long *destLen,
	const unsigned char *source,
	unsigned long sourceLen,
	int level
){
	z_stream z;
	int status;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if((status=deflateInit2(
		&z, level , Z_DEFLATED, -MAX_WBITS, level, Z_DEFAULT_STRATEGY
	)) != Z_OK){
		return status;
	}

	z.next_in = source;
	z.avail_in = sourceLen;
	z.next_out = dest;
	z.avail_out = *destLen;

	status = deflate(&z, Z_FINISH); //Z_FULL_FLUSH
	if(status != Z_STREAM_END && status != Z_OK){
		//fprintf(stderr,"deflate: %s\n", (z.msg) ? z.msg : "???");
		return status;
	}
	*destLen-=z.avail_out;

	return deflateEnd(&z);
}

int ZEXPORT zlib_inflate(
	unsigned char *dest,
	unsigned long *destLen,
	const unsigned char *source,
	unsigned long sourceLen
){
	z_stream z;
	int status;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if((status=inflateInit2(
		&z, -MAX_WBITS
	)) != Z_OK){
		return status;
	}

	z.next_in = source;
	z.avail_in = sourceLen;
	z.next_out = dest;
	z.avail_out = *destLen;

	for(;z.avail_out && status != Z_STREAM_END;){
		status = inflate(&z, Z_BLOCK);
		if(status==Z_BUF_ERROR)break;
		if(status != Z_STREAM_END && status != Z_OK){
			//fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
			return status;
		}
	}
	*destLen-=z.avail_out;

	return inflateEnd(&z);
}
