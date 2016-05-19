/*
zlibrawstdio:  RFC 1950 (zlib)
zlibrawstdio2: RFC 1951 (deflate)
7gzip:         RFC 1952
*/

#include "../compat.h"
#include <stdio.h>
#include <time.h>
#include "../lib/zlib/zlib.h"
#include "../lib/lzma.h"

#ifdef STANDALONE
unsigned char buf[BUFLEN];

unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24);
}
void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}
#else
#include "../lib/xutil.h"
#endif

typedef struct{
	FILE *f;
	unsigned int crc;
	unsigned int size;
}han;

static int fread2(void *_h, char *p, int n){
	han *h=(han*)_h;
	int r=fread(p,1,n,h->f);
	h->crc=crc32(h->crc,p,r);
	h->size+=r;
	return r;
}
static int fwrite2(void *h, const char *p, int n){return fwrite(p,1,n,(FILE*)h);}
static int _compress(FILE *fin,FILE *fout,int level){
	if(!lzmaOpen7z()){
		void *coder=NULL;
		lzmaCreateCoder(&coder,0x040108,1,level);
		if(coder){
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
			han h;
			h.f=fin;
			h.crc=0;
			h.size=0;
			lzmaCodeCallback(coder,&h,fread2,NULL,stdout,fwrite2,NULL);
			lzmaDestroyCoder(&coder);
			write32(buf,h.crc);
			fwrite(buf,1,4,fout);
			write32(buf,h.size);
			fwrite(buf,1,4,fout);
			fflush(fout);
		}
		lzmaClose7z();
	}else{
		char mode[]="wb0";
		mode[2]+=level;
		int readlen;
#ifdef FEOS
		gzFile gz=gzdopen(fileno(fout),mode);
		for(;(readlen=fread(buf,1,BUFLEN,stdin))>0;){
			gzwrite(gz,buf,readlen);
		}
		gzflush(gz,Z_FINISH);
#else
		gzFile gz=gzdopen(dup(fileno(fout)),mode);
		for(;(readlen=fread(buf,1,BUFLEN,stdin))>0;){
			gzwrite(gz,buf,readlen);
		}
		gzclose(gz);
#endif
	}
	return 0;
}

static int _decompress(FILE *fin,FILE *fout){
	int readlen;
#ifdef FEOS
	gzFile gz=gzdopen(fileno(fin),"rb");
	for(;(readlen=gzread(gz,buf,BUFLEN))>0;){
		fwrite(buf,1,readlen,fout);
	}
#else
	gzFile gz=gzdopen(dup(fileno(fin)),"rb");
	for(;(readlen=gzread(gz,buf,BUFLEN))>0;){
		fwrite(buf,1,readlen,fout);
	}
	gzclose(gz);
#endif
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int _7gzip(const int argc, const char **argv){
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
	fprintf(stderr,"7gzip e/d < in > out\nYou can also use -e,-c,-d.\n");
	if(!lzmaOpen7z())fprintf(stderr,"\nNote: 7-zip is AVAILABLE.\n"),lzmaClose7z();
	else fprintf(stderr,"\nNote: 7-zip is NOT available.\n");
	return -1;
}
