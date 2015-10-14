/*
zlibrawstdio:  RFC 1950 (zlib)
zlibrawstdio2: RFC 1951 (deflate)
7gzip:         RFC 1952
*/

#include <stdio.h>
#include "../lib/zlib/zlib.h"
#include "../lib/lzma.h"

#define DECOMPBUFLEN (1<<16)
#define COMPBUFLEN   (DECOMPBUFLEN|(DECOMPBUFLEN>>1))

#ifdef STANDALONE
unsigned char __compbuf[COMPBUFLEN],__decompbuf[DECOMPBUFLEN];
#else
#include "../compat.h"
extern unsigned char __compbuf[COMPBUFLEN],__decompbuf[DECOMPBUFLEN];
#endif

static int fread2(void *h, char *p, int n){return fread(p,1,n,(FILE*)h);}
static int fwrite2(void *h, const char *p, int n){return fwrite(p,1,n,(FILE*)h);}
static int _compress(FILE *fin,FILE *fout,int level){
	if(!lzmaOpen7z()){
		void *coder=NULL;
		lzmaCreateCoder(&coder,0x040108,1,level);
		if(coder){
			lzmaCodeCallback(coder,fin,fread2,fclose,fout,fwrite2,NULL);
			fflush(fout);
			lzmaDestroyCoder(&coder);
		}
		lzmaClose7z();
		return 0;
	}
	z_stream z;
	int status;
	int flush=Z_NO_FLUSH;
	//int filesize=filelength(fileno(fin));

	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if(deflateInit2(&z, level , Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY) != Z_OK){
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

static int _decompress(FILE *fin,FILE *fout){
#if 0
	if(!lzmaOpen7z()){
		void *coder=NULL;
		lzmaCreateCoder(&coder,0x040108,0,0);
		if(coder){
			lzmaCodeCallback(coder,fin,fread2,NULL,fout,fwrite2,NULL);
			lzmaDestroyCoder(&coder);
		}
		lzmaClose7z();
		return 0;
	}
#endif
	z_stream z;
	int status;
	//int filesize=filelength(fileno(fin));

	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if(inflateInit2(&z,-MAX_WBITS) != Z_OK){
		fprintf(stderr,"inflateInit: %s\n", (z.msg) ? z.msg : "???");
		return 1;
	}

	z.next_in = __compbuf;
	z.avail_in = fread(__compbuf,1,COMPBUFLEN,fin);
	z.next_out = __decompbuf;
	z.avail_out = DECOMPBUFLEN;

	for(;;){
		status = inflate(&z, Z_NO_FLUSH);
		if(status == Z_STREAM_END)break;
		if(status != Z_OK){
			fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
			return 10;
		}

		//goto next buffer
		if(z.avail_in == 0){
			z.next_in = __compbuf;
			z.avail_in = fread(__compbuf,1,COMPBUFLEN,fin);
			if(z.avail_in == 0){fprintf(stderr,"deflated data truncated.\n");return 11;}
		}
		if(z.avail_out == 0){
			fwrite(__decompbuf,1,DECOMPBUFLEN,fout);
			z.next_out = __decompbuf;
			z.avail_out = DECOMPBUFLEN;
		}
	}
	fwrite(__decompbuf,1,DECOMPBUFLEN-z.avail_out,fout);

	if(inflateEnd(&z) != Z_OK){
		fprintf(stderr,"inflateEnd: %s\n", (z.msg) ? z.msg : "???");
		return 2;
	}
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
#else
int zlibrawstdio2(const int argc, const char **argv){
#endif
	char mode,level;
	if(argc<2)goto argerror;
	mode=argv[1][0],level=argv[1][1];
	if(!mode)goto argerror;
	if(mode=='-')mode=argv[1][1],level=argv[1][2];
	if(mode!='e'&&mode!='c'&&mode!='d')goto argerror;
	if(isatty(fileno(stdin))&&isatty(fileno(stdout)))goto argerror;

	return mode=='d'?_decompress(stdin,stdout):_compress(stdin,stdout,level?level-'0':9);

argerror:
	fprintf(stderr,"zlibrawstdio2 e/d < in > out\nYou can also use -e,-c,-d.\n");
	if(!lzmaOpen7z())fprintf(stderr,"\nNote: 7-zip is AVAILABLE.\n"),lzmaClose7z();
	else fprintf(stderr,"\nNote: 7-zip is NOT available.\n");
	return -1;
}
