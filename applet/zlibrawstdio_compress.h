/*
zlibrawstdio:  RFC 1950 (zlib)
zlibrawstdio2: RFC 1951 (deflate)
7gzip:         RFC 1952
*/

#include "../compat.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "../lib/isa-l/include/igzip_lib.h"
#include "../lib/isa-l/include/crc.h"
#include "../lib/zlibutil.h"
#include "../lib/lzma.h"
#include "../lib/libdeflate/libdeflate.h"
#include "../lib/zopfli/deflate.h"
#include "../lib/slz.h"
#include "../lib/miniz/miniz.h"
// #include "../lib/zlib-ng/zlib-ng.h" // this collides with zlib.h...
#include "../lib/popt/popt.h"

#if !defined(NOIGZIP)
#define adler32 isal_adler32
#define crc32 crc32_gzip_refl
#endif

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
#include <sys/mman.h>
#endif

extern int igzip_level_size_buf[];

#define DECOMPBUFLEN (1<<16)
#define COMPBUFLEN   (DECOMPBUFLEN|(DECOMPBUFLEN>>1))

#ifdef STANDALONE
unsigned char buf[BUFLEN];
unsigned char __compbuf[COMPBUFLEN],__decompbuf[DECOMPBUFLEN];

unsigned int read32(const void *p);
void write32(void *p, const unsigned int n);
#if 0
unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24);
}
void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}
#endif
void write32be(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[3]=n&0xff,x[2]=(n>>8)&0xff,x[1]=(n>>16)&0xff,x[0]=(n>>24)&0xff;
}
#else
#include "../lib/xutil.h"
extern unsigned char __compbuf[COMPBUFLEN],__decompbuf[DECOMPBUFLEN];
#endif

typedef struct{
	FILE *f;
	unsigned int crc;
	unsigned int size;
}han;

#if defined(ZLIBRAWSTDIO_COMPRESS_DEFLATE)
static int fread2(void *_h, char *p, int n){
	han *h=(han*)_h;
	return fread(p,1,n,h->f);
}
#elif defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
static int fread2(void *_h, char *p, int n){
	han *h=(han*)_h;
	int r=fread(p,1,n,h->f);
	h->crc=adler32(h->crc,p,r);
	return r;
}
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
static int fread2(void *_h, char *p, int n){
	han *h=(han*)_h;
	int r=fread(p,1,n,h->f);
	h->crc=crc32(h->crc,p,r);
	h->size+=r;
	return r;
}
#else
#error ZLIBRAWSTDIO_COMPRESS value wrong
#endif
static int fwrite2(void *h, const char *p, int n){return fwrite(p,1,n,(FILE*)h);}

#if defined(ZLIBRAWSTDIO_COMPRESS_DEFLATE)
static void writeHeader(FILE *fout){}
static void writeTrailer(FILE *fout,unsigned int crc,unsigned int size){}
#elif defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
static void writeHeader(FILE *fout){
	fputc(0x78,fout);
	fputc(0xda,fout);
}
static void writeTrailer(FILE *fout,unsigned int crc,unsigned int size){
	unsigned char buf[4];
	write32be(buf,crc);
	fwrite(buf,1,4,fout);
}
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
static void writeHeader(FILE *fout){
	unsigned char buf[4];
	fputc(0x1f,fout);
	fputc(0x8b,fout);
	fputc(0x08,fout);
	fputc(0x00,fout);
	time_t t=time(NULL);
	write32(buf,t);
	fwrite(buf,1,4,fout);
	fputc(0x02,fout);
	fputc(0x00,fout); //0x00=Win, 0x03==Unix
}
static void writeTrailer(FILE *fout,unsigned int crc,unsigned int size){
	unsigned char buf[4];
	write32(buf,crc);
	fwrite(buf,1,4,fout);
	write32(buf,size);
	fwrite(buf,1,4,fout);
}
#endif

static int _compress(FILE *fin,FILE *fout,int level,int method){
	if(method==DEFLATE_7ZIP){
		void *coder=NULL;
		lzmaCreateCoder(&coder,0x040108,1,level);
		if(coder){
			han h;
			h.f=fin;
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
			h.crc=1;
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
			h.crc=0;
			h.size=0;
#endif
			writeHeader(fout);
			lzmaCodeCallback(coder,&h,fread2,fin==stdin ? NULL : (tClose)fclose,stdout,fwrite2,fout==stdout ? NULL : (tClose)fclose);
			lzmaDestroyCoder(&coder);
			writeTrailer(fout,h.crc,h.size);
			fflush(fout);
		}
		lzmaClose7z();
		return 0;
	}

	if(method==DEFLATE_IGZIP){
#if defined(NOIGZIP)
		return -1;
#else
		writeHeader(fout);
		unsigned int crc=0;
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
		crc=1;
#endif
		unsigned int size=0;

		struct isal_zstream z;
		isal_deflate_init(&z);
		z.next_in = __decompbuf;
		z.avail_in = fread(__decompbuf,1,DECOMPBUFLEN,fin);
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
		crc=adler32(crc,__decompbuf,z.avail_in);
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
		crc=crc32(crc,__decompbuf,z.avail_in);
#endif
		size+=z.avail_in;
		z.next_out = __compbuf;
		z.avail_out = COMPBUFLEN;
		z.flush = NO_FLUSH;
		z.gzip_flag = 0;
		z.level = level-1;
		z.level_buf_size = igzip_level_size_buf[z.level];
		z.level_buf = malloc(z.level_buf_size);
		if(z.avail_in < DECOMPBUFLEN){
			z.end_of_stream=1;
			//z.flush=FULL_FLUSH;
		}

		for(;;){
			int status = isal_deflate(&z);
			if(status != COMP_OK){
				fprintf(stderr,"isal_deflate: %d\n", status);
				return 10;
			}
			if(z.internal_state.state == ZSTATE_END)break;

			//goto next buffer
			if(z.avail_in == 0){
				//if(flush==Z_FINISH){fprintf(stderr,"failed to complete deflation.\n");return 11;}
				z.next_in = __decompbuf;
				z.avail_in = fread(__decompbuf,1,DECOMPBUFLEN,fin);
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
				crc=adler32(crc,__decompbuf,z.avail_in);
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
				crc=crc32(crc,__decompbuf,z.avail_in);
#endif
				size+=z.avail_in;
				if(z.avail_in < DECOMPBUFLEN){
					z.end_of_stream=1;
					//z.flush=FULL_FLUSH;
				}
			}
			if(z.avail_out == 0){
				fwrite(__compbuf,1,COMPBUFLEN,fout);
				z.next_out = __compbuf;
				z.avail_out = COMPBUFLEN;
			}
		}
		fwrite(__compbuf,1,COMPBUFLEN-z.avail_out,fout);

		writeTrailer(fout,crc,size);
		fflush(fout);
		free(z.level_buf);
		return 0;
#endif
	}

	if(method==DEFLATE_SLZ){
		writeHeader(fout);
		unsigned int crc=0;
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
		crc=1;
#endif
		unsigned int size=0;

		struct slz_stream strm;
		slz_init(&strm, level, SLZ_FMT_DEFLATE);
		for(;;){
			int readlen = fread(__decompbuf,1,DECOMPBUFLEN,fin);
			size += readlen;
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
			crc=adler32(crc,__decompbuf,readlen);
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
			crc=crc32(crc,__decompbuf,readlen);
#endif
			int writelen = slz_encode(&strm,__compbuf,__decompbuf,readlen,1);
			fwrite(__compbuf,1,writelen,fout);

			if(readlen<DECOMPBUFLEN){
				//int writelen = slz_encode(&strm,__compbuf,__decompbuf,readlen,0);
				//fwrite(__compbuf,1,writelen,fout);
				int writelen = slz_finish(&strm,__compbuf);
				fwrite(__compbuf,1,writelen,fout);
				break;
			}
		}

		writeTrailer(fout,crc,size);
		fflush(fout);
		return 0;
	}

	// the mmap logic is from https://github.com/ebiggers/libdeflate/blob/master/programs/prog_util.c
	if(method==DEFLATE_LIBDEFLATE){
		long long siz = filelengthi64(fileno(fin));
		if(!siz){
			fprintf(stderr,"empty file is not supported (possibly stdinasfile is required)\n");
			return 1;
		}
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
		HANDLE *h = CreateFileMapping(_get_osfhandle(fileno(fin)), NULL, PAGE_READONLY, 0, 0, NULL);
		char *mem = MapViewOfFile(h, FILE_MAP_READ, 0, 0, siz);
#else
		char *mem = mmap(NULL, siz, PROT_READ, MAP_PRIVATE, fileno(fin), 0);
#endif
		if(!mem){
			fprintf(stderr,"failed mmap\n");
			return 1;
		}
		long long maxcompsize = libdeflate_deflate_compress_bound(NULL, siz) + 18;
		char *compdata = malloc(maxcompsize);

		{
			zlibutil_buffer zlibbuf;
			memset(&zlibbuf, 0, sizeof(zlibutil_buffer));
			zlibbuf.dest = compdata;
			zlibbuf.destLen = maxcompsize;
			zlibbuf.source = mem;
			zlibbuf.sourceLen = siz;
			zlibbuf.encode = 1;
			zlibbuf.level = level;
			zlibbuf.func = libdeflate_deflate;
#if defined(ZLIBRAWSTDIO_COMPRESS_DEFLATE)
#elif defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
			zlibbuf.rfc1950 = 1;
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
			zlibbuf.rfc1952 = 1;
#endif
			zlibutil_buffer_code(&zlibbuf);
			fwrite(zlibbuf.dest, 1, zlibbuf.destLen, fout);
			fflush(fout);
		}
		free(compdata);
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
		UnmapViewOfFile(mem);
		CloseHandle(h);
#else
		munmap(mem, siz);
#endif
		return 0;
	}

	if(method==DEFLATE_ZOPFLI){
		long long siz = filelengthi64(fileno(fin));
		if(!siz){
			fprintf(stderr,"empty file is not supported (possibly stdinasfile is required)\n");
			return 1;
		}
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
		HANDLE *h = CreateFileMapping(_get_osfhandle(fileno(fin)), NULL, PAGE_READONLY, 0, 0, NULL);
		char *mem = MapViewOfFile(h, FILE_MAP_READ, 0, 0, siz);
#else
		char *mem = mmap(NULL, siz, PROT_READ, MAP_PRIVATE, fileno(fin), 0);
#endif
		if(!mem){
			fprintf(stderr,"failed mmap\n");
			return 1;
		}
		long long maxcompsize = libdeflate_deflate_compress_bound(NULL, siz) + 18;
		char *compdata = malloc(maxcompsize);

		{
			zlibutil_buffer zlibbuf;
			memset(&zlibbuf, 0, sizeof(zlibutil_buffer));
			zlibbuf.dest = compdata;
			zlibbuf.destLen = maxcompsize;
			zlibbuf.source = mem;
			zlibbuf.sourceLen = siz;
			zlibbuf.encode = 1;
			zlibbuf.level = level;
			zlibbuf.func = zopfli_deflate;
#if defined(ZLIBRAWSTDIO_COMPRESS_DEFLATE)
#elif defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
			zlibbuf.rfc1950 = 1;
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
			zlibbuf.rfc1952 = 1;
#endif
			zlibutil_buffer_code(&zlibbuf);
			fwrite(zlibbuf.dest, 1, zlibbuf.destLen, fout);
			fflush(fout);
		}
		free(compdata);
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
		UnmapViewOfFile(mem);
		CloseHandle(h);
#else
		munmap(mem, siz);
#endif
		return 0;
	}

	if(method==DEFLATE_KZIP){
		long long siz = filelengthi64(fileno(fin));
		if(!siz){
			fprintf(stderr,"empty file is not supported (possibly stdinasfile is required)\n");
			return 1;
		}
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
		HANDLE *h = CreateFileMapping(_get_osfhandle(fileno(fin)), NULL, PAGE_READONLY, 0, 0, NULL);
		char *mem = MapViewOfFile(h, FILE_MAP_READ, 0, 0, siz);
#else
		char *mem = mmap(NULL, siz, PROT_READ, MAP_PRIVATE, fileno(fin), 0);
#endif
		if(!mem){
			fprintf(stderr,"failed mmap\n");
			return 1;
		}
		long long maxcompsize = libdeflate_deflate_compress_bound(NULL, siz) + 18;
		char *compdata = malloc(maxcompsize);

		{
			zlibutil_buffer zlibbuf;
			memset(&zlibbuf, 0, sizeof(zlibutil_buffer));
			zlibbuf.dest = compdata;
			zlibbuf.destLen = maxcompsize;
			zlibbuf.source = mem;
			zlibbuf.sourceLen = siz;
			zlibbuf.encode = 1;
			zlibbuf.level = level;
			zlibbuf.func = kzip_deflate;
#if defined(ZLIBRAWSTDIO_COMPRESS_DEFLATE)
#elif defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
			zlibbuf.rfc1950 = 1;
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
			zlibbuf.rfc1952 = 1;
#endif
			zlibutil_buffer_code(&zlibbuf);
			fwrite(zlibbuf.dest, 1, zlibbuf.destLen, fout);
			fflush(fout);
		}
		free(compdata);
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
		UnmapViewOfFile(mem);
		CloseHandle(h);
#else
		munmap(mem, siz);
#endif
		return 0;
	}

	if(method==DEFLATE_MINIZ){
		writeHeader(fout);
		unsigned int crc=0;
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
		crc=1;
#endif
		unsigned int size=0;

		mz_stream z;
		int status;
		int flush=MZ_NO_FLUSH;
		//long long filesize=filelengthi64(fileno(fin));

		z.zalloc = Z_NULL;
		z.zfree = Z_NULL;
		z.opaque = Z_NULL;
		//miniz does not supprt gzip mode
//#if defined(ZLIBRAWSTDIO_COMPRESS_DEFLATE)
		int windowBits = -MAX_WBITS;
//#elif defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
//		int windowBits = MAX_WBITS;
//#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
//		int windowBits = MAX_WBITS+16;
//#endif
		if(mz_deflateInit2(&z,level,MZ_DEFLATED, windowBits, 9, MZ_DEFAULT_STRATEGY) != Z_OK){
			fprintf(stderr,"deflateInit: %s\n", (z.msg) ? z.msg : "???");
			return 1;
		}

		z.next_in = __decompbuf;
		z.avail_in = fread(__decompbuf,1,DECOMPBUFLEN,fin);
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
		crc=adler32(crc,__decompbuf,z.avail_in);
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
		crc=crc32(crc,__decompbuf,z.avail_in);
#endif
		size+=z.avail_in;
		z.next_out = __compbuf;
		z.avail_out = COMPBUFLEN;
		if(z.avail_in < DECOMPBUFLEN)flush=Z_FINISH;

		for(;;){
			status = mz_deflate(&z, flush);
			if(status == MZ_STREAM_END)break;
			if(status != MZ_OK){
				fprintf(stderr,"deflate: %s\n", (z.msg) ? z.msg : "???");
				return 10;
			}

			//goto next buffer
			if(z.avail_in == 0){
				//if(flush==Z_FINISH){fprintf(stderr,"failed to complete deflation.\n");return 11;}
				z.next_in = __decompbuf;
				z.avail_in = fread(__decompbuf,1,DECOMPBUFLEN,fin);
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
				crc=adler32(crc,__decompbuf,z.avail_in);
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
				crc=crc32(crc,__decompbuf,z.avail_in);
#endif
				size+=z.avail_in;
				if(z.avail_in < DECOMPBUFLEN)flush=Z_FINISH;
			}
			if(z.avail_out == 0){
				fwrite(__compbuf,1,COMPBUFLEN,fout);
				z.next_out = __compbuf;
				z.avail_out = COMPBUFLEN;
			}
		}
		fwrite(__compbuf,1,COMPBUFLEN-z.avail_out,fout);
		writeTrailer(fout,crc,size);
		fflush(fout);

		if(mz_deflateEnd(&z) != Z_OK){
			fprintf(stderr,"deflateEnd: %s\n", (z.msg) ? z.msg : "???");
			return 2;
		}
		return 0;
	}

	if(method==DEFLATE_ZLIB){
		z_stream z;
		int status;
		int flush=Z_NO_FLUSH;
		//long long filesize=filelengthi64(fileno(fin));

		z.zalloc = Z_NULL;
		z.zfree = Z_NULL;
		z.opaque = Z_NULL;
#if defined(ZLIBRAWSTDIO_COMPRESS_DEFLATE)
		int windowBits = -MAX_WBITS;
#elif defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
		int windowBits = MAX_WBITS;
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
		int windowBits = MAX_WBITS+16;
#endif
		if(deflateInit2(&z,level,Z_DEFLATED, windowBits,
#if NO_EMULATEGZIP
			9,
#else
			8,
#endif
			Z_DEFAULT_STRATEGY) != Z_OK){
			fprintf(stderr,"deflateInit: %s\n", (z.msg) ? z.msg : "???");
			return 1;
		}

		z.next_in = __decompbuf;
		z.avail_in = fread(__decompbuf,1,DECOMPBUFLEN,fin);
		z.next_out = __compbuf;
		z.avail_out = COMPBUFLEN;
		if(z.avail_in < DECOMPBUFLEN)flush=Z_FINISH;

		for(;;){
			status = deflate(&z, flush);
			if(status == Z_STREAM_END)break;
			if(status != Z_OK){
				fprintf(stderr,"deflate: %s\n", (z.msg) ? z.msg : "???");
				return 10;
			}

			//goto next buffer
			if(z.avail_in == 0){
				//if(flush==Z_FINISH){fprintf(stderr,"failed to complete deflation.\n");return 11;}
				z.next_in = __decompbuf;
				z.avail_in = fread(__decompbuf,1,DECOMPBUFLEN,fin);
				if(z.avail_in < DECOMPBUFLEN)flush=Z_FINISH;
			}
			if(z.avail_out == 0){
				fwrite(__compbuf,1,COMPBUFLEN,fout);
				z.next_out = __compbuf;
				z.avail_out = COMPBUFLEN;
			}
		}
		fwrite(__compbuf,1,COMPBUFLEN-z.avail_out,fout);

		if(deflateEnd(&z) != Z_OK){
			fprintf(stderr,"deflateEnd: %s\n", (z.msg) ? z.msg : "???");
			return 2;
		}
		return 0;
	}

	if(method==DEFLATE_STORE){
		writeHeader(fout);
		unsigned int crc=0;
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
		crc=1;
#endif
		unsigned int size=0;

    for(;;){
			int readlen = fread(__decompbuf,1,65535,fin);
			size += readlen;
#if defined(ZLIBRAWSTDIO_COMPRESS_ZLIB)
			crc=adler32(crc,__decompbuf,readlen);
#elif defined(ZLIBRAWSTDIO_COMPRESS_GZIP)
			crc=crc32(crc,__decompbuf,readlen);
#endif
      __compbuf[0] = !!(readlen < 65535);
			write16(__compbuf+1, readlen);
			write16(__compbuf+3, ~readlen);
			fwrite(__compbuf,1,5,fout);
      fwrite(__decompbuf,1,readlen,fout);
      if(readlen < 65535)break;
		}

		writeTrailer(fout,crc,size);
		fflush(fout);

		return 0;
	}

	return -1;
}

static int _decompress(FILE *fin,FILE *fout);

static int zlibrawstdio_main(const int argc, const char **argv){
	int cmode=0,mode=0;
	int zlib=0,sevenzip=0,zopfli=0,miniz=0,slz=0,libdeflate=0,zlibng=0,igzip=0,cryptopp=0,kzip=0,store=0;
	//int nthreads=1;
	//int bsize=0;
	poptContext optCon;
	int optc;

	struct poptOption optionsTable[] = {
		//{ "longname", "shortname", argInfo,      *arg,       int val, description, argment description}
		{ "stdout", 'c',         POPT_ARG_NONE,            &cmode,      0,       "stdout (currently ignored; always output to stdout)", NULL },
		{ "zlib",   'z',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,       'z',     "1-9 (default 6) zlib", "level" },
		{ "miniz",     'm',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'm',       "1-9 (default 1) miniz", "level" },
		{ "slz",     's',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    's',       "1-1 (default 1) slz", "level" },
		{ "libdeflate",   'l',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,       'l',     "1-12 (default 6) libdeflate", "level" },
		{ "7zip",     'S',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'S',       "1-9 (default 2) 7zip", "level" },
		//{ "zlibng",     'n',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'n',       "1-9 (default 6) zlibng", "level" },
		//{ "cryptopp",     'C',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'C',       "1-9 (default 6) cryptopp", "level" },
		{ "igzip",   'i',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,       'i',     "1-4 (default 1) igzip (1 becomes igzip internal level 0, 2 becomes 1, ...)", "level" },
		{ "kzip",     'K',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'K',       "1-1 (default 1) kzip", "level" },
		{ "zopfli",     'Z',         POPT_ARG_INT, &zopfli,    0,       "zopfli", "numiterations" },
		{ "store",     'T',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'T',       "1-1 (default 1) store", "level" },
		//{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		{ "decompress", 'd',         POPT_ARG_NONE,            &mode,      0,       "decompress", NULL },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "{-cz9 <dec.bin >enc.gz} or {-cd <enc.gz >dec.bin}");

	for(;(optc=poptGetNextOpt(optCon))>=0;){
		switch(optc){
			case 'z':{
				char *arg=poptGetOptArg(optCon);
				if(arg)zlib=strtol(arg,NULL,10),free(arg);
				else zlib=6;
				break;
			}
			case 'm':{
				char *arg=poptGetOptArg(optCon);
				if(arg)miniz=strtol(arg,NULL,10),free(arg);
				else miniz=1;
				break;
			}
			case 's':{
				char *arg=poptGetOptArg(optCon);
				if(arg)slz=strtol(arg,NULL,10),free(arg);
				else slz=1;
				break;
			}
			case 'l':{
				char *arg=poptGetOptArg(optCon);
				if(arg)libdeflate=strtol(arg,NULL,10),free(arg);
				else libdeflate=6;
				break;
			}
			case 'S':{
				char *arg=poptGetOptArg(optCon);
				if(arg)sevenzip=strtol(arg,NULL,10),free(arg);
				else sevenzip=2;
				break;
			}
			case 'n':{
				char *arg=poptGetOptArg(optCon);
				if(arg)zlibng=strtol(arg,NULL,10),free(arg);
				else zlibng=6;
				break;
			}
			case 'C':{
				char *arg=poptGetOptArg(optCon);
				if(arg)cryptopp=strtol(arg,NULL,10),free(arg);
				else cryptopp=6;
				break;
			}
			case 'i':{
				char *arg=poptGetOptArg(optCon);
				if(arg)igzip=strtol(arg,NULL,10),free(arg);
				else igzip=1;
				break;
			}
			case 'K':{
				char *arg=poptGetOptArg(optCon);
				if(arg)kzip=strtol(arg,NULL,10),free(arg);
				else kzip=1;
				break;
			}
			case 'T':{
				char *arg=poptGetOptArg(optCon);
				if(arg)store=strtol(arg,NULL,10),free(arg);
				else store=1;
				break;
			}
		}
	}

	int level_sum=zlib+sevenzip+zopfli+miniz+slz+libdeflate+zlibng+igzip+cryptopp+kzip+store;
	if(
		optc<-1 ||
		(!mode&&!zlib&&!sevenzip&&!zopfli&&!miniz&&!slz&&!libdeflate&&!zlibng&&!igzip&&!cryptopp&&!kzip&&!store) ||
		(mode&&(zlib||sevenzip||zopfli||miniz||slz||libdeflate||zlibng||igzip||cryptopp||kzip||store)) ||
		(!mode&&(level_sum==zlib)+(level_sum==sevenzip)+(level_sum==zopfli)+(level_sum==miniz)+(level_sum==slz)+(level_sum==libdeflate)+(level_sum==zlibng)+(level_sum==igzip)+(level_sum==cryptopp)+(level_sum==kzip)+(level_sum==store)!=1)
	){
		poptPrintHelp(optCon, stderr, 0);
		poptFreeContext(optCon);
		if(!lzmaOpen7z()){char path[768];lzmaGet7zFileName(path, 768);fprintf(stderr,"\nNote: 7-zip is AVAILABLE (%s).\n", path);lzmaClose7z();}
		else fprintf(stderr,"\nNote: 7-zip is NOT available.\n");
		return 1;
	}

#ifdef NOTIMEOFDAY
	time_t tstart,tend;
	time(&tstart);
#else
	struct timeval tstart,tend;
	gettimeofday(&tstart,NULL);
#endif
	int ret=0;
	if(mode){
		if(isatty(fileno(stdin))||isatty(fileno(stdout)))
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		poptFreeContext(optCon);

		//lzmaOpen7z();
		ret=_decompress(stdin,stdout);
		//lzmaClose7z();
	}else{
		if(isatty(fileno(stdin))||isatty(fileno(stdout)))
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		poptFreeContext(optCon);

		fprintf(stderr,"compression level = %d ",level_sum);
		if(zlib){
			fprintf(stderr,"(zlib)\n");
			ret=_compress(stdin,stdout,zlib,DEFLATE_ZLIB);
		}else if(sevenzip){
			fprintf(stderr,"(7zip)\n");
			if(lzmaOpen7z()){
				fprintf(stderr,"7-zip is NOT available.\n");
				return -1;
			}
			ret=_compress(stdin,stdout,sevenzip,DEFLATE_7ZIP);
			lzmaClose7z();
		}else if(libdeflate){
			fprintf(stderr,"(libdeflate)\n");
			ret=_compress(stdin,stdout,libdeflate,DEFLATE_LIBDEFLATE);
		}else if(igzip){
			fprintf(stderr,"(igzip)\n");
			ret=_compress(stdin,stdout,igzip,DEFLATE_IGZIP);
		}else if(miniz){
			fprintf(stderr,"(miniz)\n");
			ret=_compress(stdin,stdout,miniz,DEFLATE_MINIZ);
		}else if(slz){
			fprintf(stderr,"(slz)\n");
			ret=_compress(stdin,stdout,slz,DEFLATE_SLZ);
		}else if(zlibng){
			fprintf(stderr,"(zlibng)\n");
			ret=_compress(stdin,stdout,zlibng,DEFLATE_ZLIBNG);
		}else if(cryptopp){
			fprintf(stderr,"(cryptopp)\n");
			ret=_compress(stdin,stdout,cryptopp,DEFLATE_CRYPTOPP);
		}else if(zopfli){
			fprintf(stderr,"(zopfli)\n");
			ret=_compress(stdin,stdout,zopfli,DEFLATE_ZOPFLI);
		}else if(kzip){
			fprintf(stderr,"(kzip)\n");
			ret=_compress(stdin,stdout,kzip,DEFLATE_KZIP);
		}else if(store){
			fprintf(stderr,"(store)\n");
			ret=_compress(stdin,stdout,store,DEFLATE_STORE);
		}
	}
#ifdef NOTIMEOFDAY
	time(&tend);
	fprintf(stderr,"ellapsed time: %d sec\n",tend-tstart);
#else
	gettimeofday(&tend,NULL);
	fprintf(stderr,"ellapsed time: %.6f sec\n",(tend.tv_sec+tend.tv_usec*0.000001)-(tstart.tv_sec+tstart.tv_usec*0.000001));
#endif
	return ret;
}
