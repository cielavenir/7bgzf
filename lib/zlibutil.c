//windowBits: 8..15: zlib, 24..31: gzip, -8..-15: raw

#include <stdbool.h>
#include "zlibutil.h"
#include "miniz.h"
#include "slz.h"
#include "libdeflate/libdeflate.h"
#include "zopfli/deflate.h"
#include "lzma.h"
//#include "zlib-ng/zlib-ng.h"

static inline void write32be(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[3]=n&0xff,x[2]=(n>>8)&0xff,x[1]=(n>>16)&0xff,x[0]=(n>>24)&0xff;
}

int zlibutil_auto_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
){
#if defined(NOIGZIP)
	return libdeflate_inflate(dest,destLen,source,sourceLen);
#else
	return igzip_inflate(dest,destLen,source,sourceLen);
#endif
}

int lzma_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
	void *coder;
	lzmaCreateCoder(&coder,0x040108,1,level);
	int status = lzmaCodeOneshot(coder,source,sourceLen,dest,destLen);
	lzmaDestroyCoder(&coder);
	return status;
}

int lzma_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
){
	void *coder;
	lzmaCreateCoder(&coder,0x040108,0,0);
	int status = lzmaCodeOneshot(coder,source,sourceLen,dest,destLen);
	lzmaDestroyCoder(&coder);
	return status;
}

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
	status=tdefl_compress(&comp,source,&_sourceLen,dest,destLen,TDEFL_FINISH);
	return status==TDEFL_STATUS_OKAY ? Z_BUF_ERROR : status==TDEFL_STATUS_DONE ? Z_OK : status;
}

int miniz_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
){
	tinfl_decompressor comp;
	int status=0;
	tinfl_init(&comp);
	size_t _sourceLen=sourceLen;
	if(status)return status;
	status=tinfl_decompress(&comp,source,&_sourceLen,dest,dest,destLen,TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
	return status;
}

static bool slz_initialized=false;
void slz_initialize(){
	if(!slz_initialized){
		slz_prepare_dist_table();
		slz_initialized=true;
	}
}

// for now need to make sure destLen is large enough //
int slz_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
	size_t destLenOrig = *destLen;
	struct slz_stream strm;
	slz_init(&strm, level, SLZ_FMT_DEFLATE);
	*destLen=slz_encode(&strm,dest,source,sourceLen,0);
	if(*destLen>destLenOrig)return Z_BUF_ERROR;
	*destLen+=slz_finish(&strm,dest+*destLen);
	if(*destLen>destLenOrig)return Z_BUF_ERROR;
	return Z_OK;
}

int libdeflate_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
	struct libdeflate_compressor *comp=(struct libdeflate_compressor*)libdeflate_alloc_compressor(level);
	size_t siz=libdeflate_deflate_compress(comp,source,sourceLen,dest,*destLen);
	libdeflate_free_compressor(comp);
	if(!siz)return !Z_OK;
	*destLen=siz;
	return Z_OK;
}

int libdeflate_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
){
	struct libdeflate_decompressor *comp=(struct libdeflate_decompressor*)libdeflate_alloc_decompressor();
	enum libdeflate_result r=libdeflate_deflate_decompress(comp,source,sourceLen,dest,*destLen,destLen);
	libdeflate_free_decompressor(comp);
	return r;
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

int zlib_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
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

int zlib_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
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

zlibutil_buffer *zlibutil_buffer_allocate(size_t destSiz, size_t sourceSiz){
	zlibutil_buffer *zlibbuf=(zlibutil_buffer*)calloc(1,sizeof(zlibutil_buffer));
	if(!zlibbuf)return zlibbuf;
	zlibbuf->dest = (unsigned char*)malloc(destSiz);
	zlibbuf->destLen = destSiz;
	zlibbuf->source = (unsigned char*)malloc(sourceSiz);
	zlibbuf->sourceLen = sourceSiz;
	return zlibbuf;
}
zlibutil_buffer *zlibutil_buffer_code(zlibutil_buffer *zlibbuf){
	if(!zlibbuf->encode){
		zlibbuf->ret = ((zlibutil_code_dec)zlibbuf->func)(zlibbuf->dest,&zlibbuf->destLen,zlibbuf->source,zlibbuf->sourceLen);
	}else{
		if(zlibbuf->rfc1950){
			zlibbuf->destLen-=6;
			zlibbuf->dest+=2;
		}
		zlibbuf->ret = ((zlibutil_code_enc)zlibbuf->func)(zlibbuf->dest,&zlibbuf->destLen,zlibbuf->source,zlibbuf->sourceLen,zlibbuf->level);
		if(zlibbuf->rfc1950 && !zlibbuf->ret){
			write32be(zlibbuf->dest+zlibbuf->destLen,adler32(1,zlibbuf->source,zlibbuf->sourceLen));
			zlibbuf->dest-=2;
			zlibbuf->dest[0]=0x78;
			zlibbuf->dest[1]=0xda;
			zlibbuf->destLen+=6;
		}
	}
	return zlibbuf;
}
void zlibutil_buffer_free(zlibutil_buffer* zlibbuf){if(zlibbuf){free(zlibbuf->dest);zlibbuf->dest=NULL;free(zlibbuf->source);zlibbuf->source=NULL;free(zlibbuf);}}
