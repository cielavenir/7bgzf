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

#if !defined(__cplusplus) && !defined(min)
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define fourcc(a,b,c,d) ((u32)(a) | ((u32)(b)<<8) | ((u32)(c)<<16) | ((u32)(d)<<24))

unsigned int read32be(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[3]|(x[2]<<8)|(x[1]<<16)|(x[0]<<24);
}
void write32be(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[3]=n&0xff,x[2]=(n>>8)&0xff,x[1]=(n>>16)&0xff,x[0]=(n>>24)&0xff;
}

unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|(x[3]<<24);
}
void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
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

#define MAX_IDAT 1024
static unsigned char* tbl[MAX_IDAT];
static unsigned int tbl_size[MAX_IDAT];

static int _compress(FILE *in, FILE *out, int level, bool strip){

#ifndef FEOS
	ZopfliOptions options;
if(level>9){
	ZopfliInitOptions(&options);
	options.numiterations = level/10;
}
#endif
	void* coder=NULL;
	lzmaCreateCoder(&coder,0x040108,1,level);

	fread(buf,1,8,in);
	if(memcmp(buf,"\x89PNG\x0d\x0a\x1a\x0a",8)){
		fprintf(stderr,"not PNG file\n");
		return -1;
	}
	fwrite(buf,1,8,out);

	//iterate png chunks
	int tbl_last=0;
	size_t compsize=0;
	for(;;){
		if(fread(buf,1,8,in)<8)break;
		unsigned int len=read32be(buf);
		unsigned int type=read32(buf+4);
		if(level && type==fourcc('I','D','A','T')){
			//recompress
			if(tbl_last==MAX_IDAT){
				fprintf(stderr,"Sorry, IDAT is limited to %d.\n",MAX_IDAT);
				return -1;
			}
			fprintf(stderr,"IDAT data length=%d\n",len);
			tbl_size[tbl_last]=len;
			tbl[tbl_last]=(unsigned char*)malloc(len);
			fread(tbl[tbl_last],1,len,in);
			compsize+=len;
			tbl_last++;
			fread(buf,1,4,in);
		}else if(level && type==fourcc('I','E','N','D')){
			fprintf(stderr,"compressed length=%d\n",compsize);
			size_t bufsize=compsize + (compsize>>1);

			int declen=0;
			{
				z_stream z;

				z.zalloc = Z_NULL;
				z.zfree = Z_NULL;
				z.opaque = Z_NULL;

				if(inflateInit(&z) != Z_OK){
					fprintf(stderr,"inflateInit: %s\n", (z.msg) ? z.msg : "???");
					return 1;
				}

				int i=0;
				for(;i<tbl_last;i++){
					z.next_in = tbl[i];
					z.avail_in = tbl_size[i];
					int status = Z_OK;
					for(;status!=Z_STREAM_END && (i==tbl_last-1||z.avail_in);){
						z.next_out = buf;
						z.avail_out = BUFLEN;

						status = inflate(&z, Z_FINISH);
						if(status != Z_STREAM_END && status != Z_OK && status != Z_BUF_ERROR){
							fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
							return 10;
						}
						declen+=BUFLEN-z.avail_out;
					}
				}
				if(inflateEnd(&z) != Z_OK){
					fprintf(stderr,"inflateEnd: %s\n", (z.msg) ? z.msg : "???");
					return 2;
				}
			}

			fprintf(stderr,"decode length=%d\n",declen);
			unsigned char* __decompbuf=(unsigned char*)malloc(declen);
			if(!__decompbuf){
				fprintf(stderr,"Failed to allocate decompressed buffer\n");
				return -1;
			}

			{
				z_stream z;

				z.zalloc = Z_NULL;
				z.zfree = Z_NULL;
				z.opaque = Z_NULL;

				if(inflateInit(&z) != Z_OK){
					fprintf(stderr,"inflateInit: %s\n", (z.msg) ? z.msg : "???");
					return 1;
				}

				int i=0;
				z.next_out = __decompbuf;
				z.avail_out = declen;
				for(;i<tbl_last;i++){
					z.next_in = tbl[i];
					z.avail_in = tbl_size[i];

					int status = inflate(&z, Z_FINISH);
					if(status != Z_STREAM_END && status != Z_OK && status != Z_BUF_ERROR){
						fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
						return 10;
					}
					free(tbl[i]);
				}
				if(inflateEnd(&z) != Z_OK){
					fprintf(stderr,"inflateEnd: %s\n", (z.msg) ? z.msg : "???");
					return 2;
				}
			}

			unsigned char* __compbuf=(unsigned char*)malloc(bufsize);
			if(!__compbuf){
				fprintf(stderr,"Failed to allocate compressed buffer\n");
				return -1;
			}

			unsigned int adler=1;
			adler=adler32(adler,__decompbuf,declen);
			compsize=0;
			unsigned char* COMPBUF = __compbuf;
#ifndef FEOS
if(level>9){
			size_t __compsize=0;
			unsigned char bp = 0;
			COMPBUF=NULL;
			ZopfliDeflate(&options, 2 /* Dynamic block */, 1, __decompbuf, (size_t)declen, &bp, &COMPBUF, &__compsize);
			compsize=__compsize;
}else
#endif
if(coder){
			compsize=bufsize;
			int r=lzmaCodeOneshot(coder,__decompbuf,declen,COMPBUF,&compsize);
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
			z.avail_in = declen;
			z.next_out = __compbuf;
			z.avail_out = bufsize;

			status = deflate(&z, Z_FINISH);
			if(status != Z_STREAM_END && status != Z_OK){
				fprintf(stderr,"deflate: %s\n", (z.msg) ? z.msg : "???");
				return 10;
			}
			compsize=bufsize-z.avail_out;

			if(deflateEnd(&z) != Z_OK){
				fprintf(stderr,"deflateEnd: %s\n", (z.msg) ? z.msg : "???");
				return 2;
			}
}
			write32be(buf,2+compsize+4);
			unsigned int crc=0;
			write32(buf+4,fourcc('I','D','A','T'));
			crc=crc32(crc,buf+4,4);
			fwrite(buf,1,8,out);

			buf[0]=0x78,buf[1]=0xda;
			crc=crc32(crc,buf,2);
			fwrite(buf,1,2,out);
			crc=crc32(crc,COMPBUF,compsize);
			fwrite(COMPBUF,1,compsize,out);

			write32be(buf,adler);
			crc=crc32(crc,buf,4);
			write32be(buf+4,crc);
			fwrite(buf,1,8,out);
			fprintf(stderr,"recompressed length=%d\n",2+compsize+4);

			write32be(buf,0);
			write32(buf+4,fourcc('I','E','N','D'));
			write32be(buf+8,0xae426082);
			fwrite(buf,1,12,out);
#ifndef FEOS
if(level>9)free(COMPBUF);
#endif
		}else{
			bool copy=!strip || type==fourcc('I','H','D','R') || type==fourcc('P','L','T','E') || type==fourcc('t','R','N','S') || type==fourcc('I','D','A','T') || type==fourcc('I','E','N','D');
			//copy
			len+=4;
			if(copy)fwrite(buf,1,8,out);
			for(;len;){
				fread(buf,1,min(BUFLEN,len),in);
				if(copy)fwrite(buf,1,min(BUFLEN,len),out);
				len-=min(BUFLEN,len);
			}
		}
	}
	fprintf(stderr,"Done.\n");
	lzmaDestroyCoder(&coder);
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
#else
int _7png(const int argc, const char **argv){
#endif
	int mode=0;
	int level=-1;
	int zopfli=0;
	poptContext optCon;
	int optc;

	struct poptOption optionsTable[] = {
		//{ "longname", "shortname", argInfo,      *arg,       int val, description, argment description}
		{ "compress",   'c',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, &level,       'c',     "1-9 (default 2) 7zip (fallback to zlib if 7z.dll/so isn't available)", "level" },
		{ "zopfli",     'z',         POPT_ARG_INT, &zopfli,    0,       "zopfli", "numiterations" },
		//{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		{ "strip", 's',         0,            &mode,      0,       "strip", "strip unnecessary chunks" },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "-c2/-z1 [-s] < before.png > after.png (only recompression is available)");

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
		(!mode&&level==-1&&!zopfli) ||
		(level!=-1&&zopfli) ||
		(level>9)
	){
		poptPrintHelp(optCon, stderr, 0);
		poptFreeContext(optCon);

		fprintf(stderr,"\nUse \"-c0 -s\" to strip unnecessary chunks without recompression.\n");

		if(!lzmaOpen7z())fprintf(stderr,"Note: 7-zip is AVAILABLE.\n"),lzmaClose7z();
		else fprintf(stderr,"Note: 7-zip is NOT available.\n");
		return 1;
	}

	if(level<0&&zopfli)level=0;

	if(isatty(fileno(stdin))||isatty(fileno(stdout)))
		{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
	poptFreeContext(optCon);

	if(zopfli)fprintf(stderr,"(zopfli numiterations %d)\n",zopfli);
	else{
		fprintf(stderr,"compression level = %d ",level);
		if(!lzmaOpen7z())fprintf(stderr,"(7zip)\n");
		else fprintf(stderr,"(zlib)\n");
	}
	int ret=_compress(stdin,stdout,level+zopfli*10,mode); //lol
	lzmaClose7z();
	return ret;
}
