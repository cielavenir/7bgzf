#ifdef STANDALONE
#include "../compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "../lib/zlibutil.h"
unsigned char buf[BUFLEN];

#define DECOMPBUFLEN (1<<16)
#define COMPBUFLEN   (DECOMPBUFLEN|(DECOMPBUFLEN>>1))
unsigned char __compbuf[COMPBUFLEN],__decompbuf[DECOMPBUFLEN];

unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24);
}
#if 0
void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}
#endif

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
//constant phrase
long long filelengthi64(int fd){
	struct stat st;
	fstat(fd,&st);
	return st.st_size;
}
int filelength(int fd){return filelengthi64(fd);}
#endif

#else
#include "../cielbox.h"
#endif

#include "../lib/lzma.h"
#include "../lib/popt/popt.h"

typedef struct ciso_header
{
    unsigned char magic[4];         /* +00 : 'C','I','S','O'                 */
    unsigned int  header_size;      /* +04 : header size (==0x18)            */
    unsigned long long int total_bytes; /* +08 : number of original data size    */
    unsigned int  block_size;       /* +10 : number of compressed block size */
    unsigned char ver;              /* +14 : version 01                      */
    unsigned char align;            /* +15 : align of index value            */
    unsigned char rsv_06[2];        /* +16 : reserved                        */
}CISO_H;

static int _compress(FILE *in, FILE *out, int level, int method, int threshold){ //align not supported yet...
	CISO_H header;
	memset(&header,0,sizeof(header));
	memcpy(header.magic,"CISO",4);
	header.header_size=sizeof(header);
	header.total_bytes=filelengthi64(fileno(in));
	header.block_size=2048;
	header.ver=1;
	header.align=0;

	long long total_block=align2p(header.block_size,header.total_bytes)/header.block_size;
	memset(buf,0,4*(total_block+1));
	fwrite(&header,1,sizeof(header),out);
	fwrite(buf,1,4*(total_block+1),out);

	void* coder=NULL;
	lzmaCreateCoder(&coder,0x040108,1,level);
	long long i=0;
	for(;i<total_block;i++){
		write32(buf+4*i,ftello(out));
		size_t readlen=fread(__decompbuf,1,header.block_size,in);
		size_t compsize=COMPBUFLEN;

		if(method==DEFLATE_ZLIB){
			int r=zlib_deflate(__compbuf,&compsize,__decompbuf,readlen,level);
			if(r){
				fprintf(stderr,"deflate %d\n",r);
				return 1;
			}
		}
		if(method==DEFLATE_7ZIP){
			if(coder){
				int r=lzmaCodeOneshot(coder,__decompbuf,readlen,__compbuf,&compsize);
				if(r){
					fprintf(stderr,"NDeflate::CCoder::Code %d\n",r);
					return 1;
				}
			}
		}
		if(method==DEFLATE_ZOPFLI){
			int r=zopfli_deflate(__compbuf,&compsize,__decompbuf,readlen,level);
			if(r){
				fprintf(stderr,"zopfli_deflate %d\n",r);
				return 1;
			}
		}
		if(method==DEFLATE_MINIZ){
			int r=miniz_deflate(__compbuf,&compsize,__decompbuf,readlen,level);
			if(r){
				fprintf(stderr,"miniz_deflate %d\n",r);
				return 1;
			}
		}
		if(method==DEFLATE_SLZ){
			int r=slz_deflate(__compbuf,&compsize,__decompbuf,readlen,level);
			if(r){
				fprintf(stderr,"slz_deflate %d\n",r);
				return 1;
			}
		}
		if(method==DEFLATE_LIBDEFLATE){
			int r=libdeflate_deflate(__compbuf,&compsize,__decompbuf,readlen,level);
			if(r){
				fprintf(stderr,"libdeflate_deflate %d\n",r);
				return 1;
			}
		}

		if(compsize>header.block_size*threshold/100){ //store
			fwrite(__decompbuf,1,readlen,out);
			write32(buf+4*i,0x80000000|read32(buf+4*i));
		}else{
			fwrite(__compbuf,1,compsize,out);
		}
		if((i+1)%256==0)fprintf(stderr,"%"LLD" / %"LLD"\r",i+1,total_block);
	}
	write32(buf+4*i,ftello(out));
	fseeko(out,sizeof(header),SEEK_SET);
	fwrite(buf,1,4*(total_block+1),out);
	fprintf(stderr,"%"LLD" / %"LLD" done.\n",i,total_block);
	lzmaDestroyCoder(&coder);
	return 0;
}

static int _decompress(FILE *in, FILE *out){
	CISO_H header;
	fread(&header,1,sizeof(header),in);
	if(memcmp(header.magic,"CISO",4)||(header.header_size&&header.header_size!=sizeof(header))){fprintf(stderr,"not CISO\n");return 1;}
	int total_block=align2p(header.block_size,header.total_bytes)/header.block_size;
	fread(buf,4,total_block+1,in);
	int i=0;
	int counter=sizeof(header)+4*(total_block+1);

	//void* coder=NULL;
	//lzmaCreateCoder(&coder,0x040108,0,0);
	for(;i<total_block;i++){
		u32 index=read32(buf+4*i);
		u32 plain=index&0x80000000;
		index&=0x7fffffff;
		u32 pos=index<<header.align;
		if(pos>counter)fread(buf+BUFLEN-(pos-counter),1,pos-counter,in); //discard

		u32 size=((read32(buf+4*(i+1))&0x7fffffff)<<header.align) - index;
		if(plain){
			fread(__decompbuf,1,size,in);counter+=size;
			fwrite(__decompbuf,1,size,out);
		}else{
			fread(__compbuf,1,size,in);counter+=size;
			size_t decompsize=header.block_size;

			int r=zlib_inflate(__decompbuf,&decompsize,__compbuf,size);
			if(r){
				fprintf(stderr,"inflate %d\n",r);
				return 1;
			}

			fwrite(__decompbuf,1,decompsize,out);
		}
		if((i+1)%256==0)fprintf(stderr,"%d / %d\r",i+1,total_block);
	}
	fprintf(stderr,"%d / %d done.\n",i,total_block);
	//lzmaDestroyCoder(&coder);
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int _7ciso(const int argc, const char **argv){
#endif
	int cmode=0,mode=0;
	int zlib=0,sevenzip=0,zopfli=0,miniz=0,slz=0,libdeflate=0,zlibng=0;
	int threshold=100;
	poptContext optCon;
	int optc;

	struct poptOption optionsTable[] = {
		//{ "longname", "shortname", argInfo,      *arg,       int val, description, argment description}
		{ "stdout", 'c',         POPT_ARG_NONE,            &cmode,      0,       "stdout (currently ignored)", NULL },
		{ "zlib",   'z',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,       'z',     "1-9 (default 6) zlib", "level" },
		{ "miniz",     'm',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'm',       "1-9 (default 1) miniz", "level" },
		{ "slz",     's',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    's',       "1-1 (default 1) slz", "level" },
		{ "libdeflate",     'l',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'l',       "1-12 (default 6) libdeflate", "level" },
		{ "7zip",     'S',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'S',       "1-9 (default 2) 7zip", "level" },
		{ "zlibng",     'n',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'n',       "1-9 (default 6) zlibng", "level" },
		{ "zopfli",     'Z',         POPT_ARG_INT, &zopfli,    0,       "zopfli", "numiterations" },
		{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		{ "decompress", 'd',         POPT_ARG_NONE,            &mode,      0,       "decompress", NULL },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "{-z9 dec.iso enc.cso} or {-cd <enc.cso >dec.iso}");

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
				else libdeflate=1;
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
		}
	}

	int level_sum=zlib+sevenzip+zopfli+miniz+slz+libdeflate+zlibng;
	if(
		optc<-1 ||
		(!mode&&!zlib&&!sevenzip&&!zopfli&&!miniz&&!slz&&!libdeflate&&!zlibng) ||
		(mode&&(zlib||sevenzip||zopfli||miniz||slz||libdeflate||zlibng)) ||
		(!mode&&(level_sum==zlib)+(level_sum==sevenzip)+(level_sum==zopfli)+(level_sum==miniz)+(level_sum==slz)+(level_sum==libdeflate)+(level_sum==zlibng)!=1)
	){
		poptPrintHelp(optCon, stderr, 0);
		poptFreeContext(optCon);
		if(!lzmaOpen7z())fprintf(stderr,"\nNote: 7-zip is AVAILABLE.\n"),lzmaClose7z();
		else fprintf(stderr,"\nNote: 7-zip is NOT available.\n");
		return 1;
	}

	if(mode){
		if(isatty(fileno(stdin))||isatty(fileno(stdout)))
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		poptFreeContext(optCon);
		//lzmaOpen7z();
		int ret=_decompress(stdin,stdout);
		//lzmaClose7z();
		return ret;
	}else{
		if(threshold<10)threshold=10;
		if(threshold>100)threshold=100;

		const char *fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *in=fopen(fname,"rb");
		if(!in){fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);return 2;}
		fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *out=fopen(fname,"wb");
		if(!out){fclose(in);fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);return 2;}
		poptFreeContext(optCon);

		fprintf(stderr,"compression level = %d ",level_sum);
		int ret=0;
		if(zlib){
			fprintf(stderr,"(zlib)\n");
			ret=_compress(in,out,zlib,DEFLATE_ZLIB,threshold);
		}else if(sevenzip){
			fprintf(stderr,"(7zip)\n");
			if(lzmaOpen7z()){
				fprintf(stderr,"7-zip is NOT available.\n");
				return -1;
			}
			ret=_compress(in,out,sevenzip,DEFLATE_7ZIP,threshold);
			lzmaClose7z();
		}else if(zopfli){
			fprintf(stderr,"(zopfli)\n");
			ret=_compress(in,out,zopfli,DEFLATE_ZOPFLI,threshold);
		}else if(miniz){
			fprintf(stderr,"(miniz)\n");
			ret=_compress(in,out,miniz,DEFLATE_MINIZ,threshold);
		}else if(slz){
			fprintf(stderr,"(slz)\n");
			ret=_compress(in,out,slz,DEFLATE_SLZ,threshold);
		}else if(libdeflate){
			fprintf(stderr,"(libdeflate)\n");
			ret=_compress(in,out,libdeflate,DEFLATE_LIBDEFLATE,threshold);
		}else if(zlibng){
			fprintf(stderr,"(zlibng)\n");
			ret=_compress(stdin,stdout,zlibng,DEFLATE_ZLIBNG,threshold);
		}
		fclose(in),fclose(out);
		return ret;
	}
}
