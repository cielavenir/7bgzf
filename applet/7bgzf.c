#ifdef STANDALONE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <zlib.h>
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long int u64;
#define align2p(p,i) (((i)+((p)-1))&~((p)-1))

#define BUFLEN (1<<22)
unsigned char buf[BUFLEN];
#define cbuf ((char*)buf)

#define DECOMPBUFLEN (1<<16)
#define COMPBUFLEN   (DECOMPBUFLEN|(DECOMPBUFLEN>>1))
unsigned char __compbuf[COMPBUFLEN],__decompbuf[DECOMPBUFLEN];

unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|(x[3]<<24);
}
void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}
void write16(void *p, const unsigned short n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff;
}

#if defined(WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
int filelength(int fd){ //constant phrase
	struct stat st;
	fstat(fd,&st);
	return st.st_size;
}
#endif

#else
#include "../cielbox.h"
#endif

#include "../lib/zopfli/deflate.h"
#include "../lib/lzma.h"
#include "../lib/popt/popt.h"

static int _compress(FILE *in, FILE *out, int level){
	int block_size=65536-4096;

#ifndef FEOS
	ZopfliOptions options;
if(level>9){
	ZopfliInitOptions(&options);
	options.numiterations = level/10;
}
#endif
	void* coder=NULL;
	lzmaCreateCoder(&coder,0x040108,1,level);
	int i=0;
	for(;/*i<total_block*/;i++){
		unsigned int crc=0;
		size_t readlen=fread(__decompbuf,1,block_size,in);
		if(readlen==0 || readlen==-1)break;
		crc=crc32(crc,__decompbuf,readlen);
		size_t compsize=0;
		unsigned char* COMPBUF = __compbuf;
#ifndef FEOS
if(level>9){
		size_t __compsize=0;
		unsigned char bp = 0;
		COMPBUF=NULL;
		ZopfliDeflate(&options, 2 /* Dynamic block */, 1, __decompbuf, (size_t)readlen, &bp, &COMPBUF, &__compsize);
		compsize=__compsize;
}else
#endif
if(coder){
		compsize=COMPBUFLEN;
		int r=lzmaCodeOneshot(coder,__decompbuf,readlen,COMPBUF,&compsize);
		if(r){
			fprintf(stderr,"NDeflate::CCoder::Code %d\n",r);
			return 1;
		}
}else{
		z_stream z;
		int status;
		//int flush=Z_NO_FLUSH;

		z.zalloc = Z_NULL;
		z.zfree = Z_NULL;
		z.opaque = Z_NULL;

		if(deflateInit2(&z, level , Z_DEFLATED, -MAX_WBITS, level, Z_DEFAULT_STRATEGY) != Z_OK){
			fprintf(stderr,"deflateInit: %s\n", (z.msg) ? z.msg : "???");
			return 1;
		}

		z.next_in = __decompbuf;
		z.avail_in = readlen;
		z.next_out = __compbuf;
		z.avail_out = COMPBUFLEN;

		status = deflate(&z, Z_FINISH);
		if(status != Z_STREAM_END && status != Z_OK){
			fprintf(stderr,"deflate: %s\n", (z.msg) ? z.msg : "???");
			return 10;
		}
		compsize=COMPBUFLEN-z.avail_out;

		if(deflateEnd(&z) != Z_OK){
			fprintf(stderr,"deflateEnd: %s\n", (z.msg) ? z.msg : "???");
			return 2;
		}
}
		fwrite("\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\x03",1,10,out);
		//extra field
		fwrite("\x06\0BC\x02\x00",1,6,out);
		size_t compsize_record=18+compsize+8-1; // <=65535
		write16(buf,compsize_record);
		fwrite(buf,1,2,out);
		fwrite(COMPBUF,1,compsize,out);
		write32(buf,crc);
		write32(buf+4,readlen);
		fwrite(buf,1,8,out);


#ifndef FEOS
if(level>9)free(COMPBUF);
#endif
		if((i+1)%64==0)fprintf(stderr,"%d\r",i+1);
	}
	fprintf(stderr,"%d done.\n",i);
	lzmaDestroyCoder(&coder);
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
#else
int _7bgzf(const int argc, const char **argv){
#endif
	int mode=0;
	int level=0;
	int zopfli=0;
	poptContext optCon;
	int optc;

	struct poptOption optionsTable[] = {
		//{ "longname", "shortname", argInfo,      *arg,       int val, description, argment description}
		{ "compress",   'c',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, &level,       'c',     "1-9 (default 2) 7zip (fallback to zlib if 7z.dll/so isn't available)", "level" },
		{ "zopfli",     'z',         POPT_ARG_INT, &zopfli,    0,       "zopfli", "numiterations" },
		//{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		//{ "decompress", 'd',         0,            &mode,      0,       "decompress", NULL },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "-c2/-z1 < dec.bin > enc.bgz (for decompression use 7gzip)");

	for(;(optc=poptGetNextOpt(optCon))>=0;){
		switch(optc){
			case 'c':{
				char *arg=poptGetOptArg(optCon);
				if(arg)level=strtol(arg,NULL,10);
				else level=2;
				break;
			}
		}
	}

	if(
		(optc<-1) ||
		(!mode&&!level&&!zopfli) ||
		(level&&zopfli) ||
		(mode&&(level||zopfli)) ||
		(level>9)
	){
		poptPrintHelp(optCon, stderr, 0);
		poptFreeContext(optCon);
		if(!lzmaOpen7z())fprintf(stderr,"\nNote: 7-zip is AVAILABLE.\n"),lzmaClose7z();
		else fprintf(stderr,"\nNote: 7-zip is NOT available.\n");
		return 1;
	}

	if(mode){
		//if(isatty(fileno(stdin))||isatty(fileno(stdout)))
		//	{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		//poptFreeContext(optCon);
		//lzmaOpen7z();
		int ret=-1;//_decompress(stdin,stdout);
		//lzmaClose7z();
		return ret;
	}else{
		if(isatty(fileno(stdin))||isatty(fileno(stdout)))
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		poptFreeContext(optCon);

		if(zopfli)fprintf(stderr,"(zopfli numiterations %d)\n",zopfli);
		else{
			fprintf(stderr,"compression level = %d ",level);
			if(!lzmaOpen7z())fprintf(stderr,"(7zip)\n");
			else fprintf(stderr,"(zlib)\n");
		}
		int ret=_compress(stdin,stdout,level+zopfli*10); //lol
		lzmaClose7z();
		return ret;
	}
}
