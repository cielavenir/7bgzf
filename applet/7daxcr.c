// parallel compression is supported.
// parallel decompression has difficulty due to uncompressed block expression.

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

//#define LEGACY

#include "../lib/lzma.h"
#include "../lib/popt/popt.h"

#ifdef NOTIMEOFDAY
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifndef BGZF_ST
#include <pthread.h>
#endif

typedef struct
{
	unsigned char magic[4];		/* +00 : 'D','A','X', 0                            */
	unsigned int total_bytes;	/* +04 : Original data size                        */
	unsigned int ver;		/* +08 : Version 0x00000001                        */
	unsigned int nNCareas;		/* +12 : Number of non-compressed areas            */
	unsigned int reserved[4];	/* +16 : Reserved for future uses                  */
} DAX_H;
#define DAX_HEADER_SIZE (0x20)
#define DAX_BLOCK_SIZE  (8192)

static int _compress(FILE *in, FILE *out, int level, int method, int nthreads){
	DAX_H header;
	memset(&header,0,sizeof(header));
	memcpy(header.magic,"DAX\0",4);
	header.total_bytes=filelengthi64(fileno(in)); //uint32? lol
	header.ver=1;

	int total_block=align2p(DAX_BLOCK_SIZE,header.total_bytes)/DAX_BLOCK_SIZE;
	unsigned int *indexes=(unsigned int*)buf;
	unsigned short *sizes=(unsigned short*)(indexes+total_block);
	fwrite(&header,1,sizeof(header),out);
	fwrite(indexes,4,total_block,out);
	fwrite(sizes,2,total_block,out);

	//void* coder=NULL;
	//lzmaCreateCoder(&coder,0x040108,1,level);

	const int chkpoint_interval=1024;
	int chkpoint=chkpoint_interval;
#ifndef BGZF_ST
	pthread_t *threads=(pthread_t*)alloca(sizeof(pthread_t)*nthreads);
#else
	nthreads=1;
#endif
	int i=0;
	for(;i<total_block;i+=nthreads){
		zlibutil_buffer *zlibbuf_main_thread=NULL;
		int j=0;
		for(;i+j<total_block && j<nthreads;j++){
			zlibutil_buffer *zlibbuf = zlibutil_buffer_allocate(DAX_BLOCK_SIZE+(DAX_BLOCK_SIZE>>1), DAX_BLOCK_SIZE);
			zlibbuf->sourceLen=fread(zlibbuf->source,1,DAX_BLOCK_SIZE,in);
			if(zlibbuf->sourceLen==-1)zlibbuf->sourceLen=0;
			if(zlibbuf->sourceLen==0){zlibutil_buffer_free(zlibbuf);break;}
			if(method==DEFLATE_ZLIB){
				zlibbuf->func = zlib_deflate;
			}else if(method==DEFLATE_7ZIP){
				zlibbuf->func = lzma_deflate;
			}else if(method==DEFLATE_ZOPFLI){
				zlibbuf->func = zopfli_deflate;
			}else if(method==DEFLATE_MINIZ){
				zlibbuf->func = miniz_deflate;
			}else if(method==DEFLATE_SLZ){
				slz_initialize();
				zlibbuf->func = slz_deflate;
			}else if(method==DEFLATE_LIBDEFLATE){
				zlibbuf->func = libdeflate_deflate;
			}else if(method==DEFLATE_ZLIBNG){
				zlibbuf->func = zlibng_deflate;
			}else if(method==DEFLATE_IGZIP){
				zlibbuf->func = igzip_deflate;
			}else if(method==DEFLATE_CRYPTOPP){
				zlibbuf->func = cryptopp_deflate;
			}else if(method==DEFLATE_KZIP){
				zlibbuf->func = kzip_deflate;
			}else if(method==DEFLATE_STORE){
				zlibbuf->func = store_deflate;
			}
			zlibbuf->encode = 1;
			zlibbuf->rfc1950 = 1;
			zlibbuf->level = level;
			if(j<nthreads-1){
#ifndef BGZF_ST
				pthread_create(&threads[j],NULL,(void*(*)(void*))zlibutil_buffer_code,zlibbuf);
#endif
			}else{
				zlibbuf_main_thread=zlibbuf;
				zlibutil_buffer_code(zlibbuf);
			}
		}
		if(!j)break;
		for(int j0=0;j0<j;j0++){
			zlibutil_buffer *zlibbuf;
			if(j0<nthreads-1){
#ifndef BGZF_ST
				pthread_join(threads[j0],(void**)&zlibbuf);
#endif
			}else{
				zlibbuf=zlibbuf_main_thread;
			}
			if(zlibbuf->ret){
				if(method==DEFLATE_ZLIB){
					fprintf(stderr,"deflate %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_7ZIP){
					fprintf(stderr,"NDeflate::CCoder::Code %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_ZOPFLI){
					fprintf(stderr,"zopfli_deflate %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_MINIZ){
					fprintf(stderr,"miniz_deflate %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_SLZ){
					fprintf(stderr,"slz_deflate %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_LIBDEFLATE){
					fprintf(stderr,"libdeflate_deflate %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_ZLIBNG){
					fprintf(stderr,"zng_deflate %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_IGZIP){
					fprintf(stderr,"isal_deflate %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_CRYPTOPP){
					fprintf(stderr,"CryptoPP::Deflator::Put %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_KZIP){
					fprintf(stderr,"kzip %d\n",zlibbuf->ret);
				}else if(method==DEFLATE_STORE){
					fprintf(stderr,"store_deflate %d\n",zlibbuf->ret);
				}
				zlibutil_buffer_free(zlibbuf);
				return 1;
			}
			indexes[i+j0]=ftello(out);
			fwrite(zlibbuf->dest,1,zlibbuf->destLen,out);
			sizes[i+j0]=zlibbuf->destLen;
			zlibutil_buffer_free(zlibbuf);
		}
		while((i+nthreads)>=chkpoint){
			fprintf(stderr,"%d / %d\r",i+nthreads,total_block);
			chkpoint+=chkpoint_interval;
		}
	}
	fseeko(out,sizeof(header),SEEK_SET);
	fwrite(indexes,4,total_block,out);
	fwrite(sizes,2,total_block,out);
	fprintf(stderr,"%d / %d done.\n",i,total_block);
	//lzmaDestroyCoder(&coder);
	return 0;
}

static int _decompress(FILE *in, FILE *out, int nthreads){
	DAX_H header;
	fread(&header,1,sizeof(header),in);
	if(memcmp(header.magic,"DAX\0",4)){fprintf(stderr,"not DAX\n");return 1;}

	int total_block=align2p(DAX_BLOCK_SIZE,header.total_bytes)/DAX_BLOCK_SIZE;
	unsigned int *indexes=(unsigned int*)buf;
	unsigned short *sizes=(unsigned short*)(indexes+total_block);
	unsigned int *ncareas=(unsigned int*)(sizes+total_block);
	fread(indexes,4,total_block,in);
	fread(sizes,2,total_block,in);
	fread(ncareas,8,header.nNCareas,in);

	int i=0,cnt_nc=0;
	for(;i<total_block;){
		if(cnt_nc<header.nNCareas && ncareas[2*cnt_nc]==i){
			int j=0;
			for(;j<ncareas[2*cnt_nc+1];j++){
				fread(__decompbuf,1,DAX_BLOCK_SIZE,in);
				fwrite(__decompbuf,1,DAX_BLOCK_SIZE,out);
				i++;
				if(i%256==0)fprintf(stderr,"%d / %d\r",i,total_block);
			}
			cnt_nc++;
		}else{
			//u32 index=indexes[i]; //original offset
			u16 size=sizes[i];
			fread(__compbuf,1,size,in);
			size_t decompsize=DAX_BLOCK_SIZE;
			zlibutil_buffer zlibbuf;
			zlibbuf.dest = __decompbuf;
			zlibbuf.destLen = decompsize;
			zlibbuf.source = __compbuf;
			zlibbuf.sourceLen = size;
			zlibbuf.encode = 0;
			zlibbuf.rfc1950 = 1;
			zlibbuf.func = zlibutil_auto_inflate;
			zlibutil_buffer_code(&zlibbuf);

			int r=zlibbuf.ret;
			if(r){
				fprintf(stderr,"inflate %d\n",r);
				return 1;
			}

			fwrite(zlibbuf.dest,1,zlibbuf.destLen,out);

			i++;
			if(i%256==0)fprintf(stderr,"%d / %d\r",i,total_block);
		}
	}
	fprintf(stderr,"%d / %d done.\n",i,total_block);
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int _7daxcr(const int argc, const char **argv){
#endif
	int cmode=0,mode=0;
	int zlib=0,sevenzip=0,zopfli=0,miniz=0,slz=0,libdeflate=0,zlibng=0,igzip=0,cryptopp=0,kzip=0,store=0;
	int threshold=100,nthreads=1;
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
		{ "cryptopp",     'C',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'C',       "1-9 (default 6) cryptopp", "level" },
		{ "igzip",     'i',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'i',       "1-4 (default 1) igzip (1 becomes igzip internal level 0, 2 becomes 1, ...)", "level" },
		{ "kzip",     'K',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'K',       "1-1 (default 1) kzip", "level" },
		{ "zopfli",     'Z',         POPT_ARG_INT, &zopfli,    0,       "zopfli", "numiterations" },
		{ "store",     'T',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'T',       "1-1 (default 1) store", "level" },
		//{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		{ "threads",     '@',         POPT_ARG_INT, &nthreads,    0,       "threads", "threads" },
		{ "decompress", 'd',         POPT_ARG_NONE,            &mode,      0,       "decompress", NULL },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "{-z9 dec.iso enc.dax} or {-cd <enc.dax >dec.iso}");

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
	if(nthreads<1)nthreads=1;

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
		ret=_decompress(stdin,stdout,nthreads);
		//lzmaClose7z();
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
		if(zlib){
			fprintf(stderr,"(zlib)\n");
			ret=_compress(in,out,zlib,DEFLATE_ZLIB,nthreads);
		}else if(sevenzip){
			fprintf(stderr,"(7zip)\n");
			if(lzmaOpen7z()){
				fprintf(stderr,"7-zip is NOT available.\n");
				return -1;
			}
			ret=_compress(in,out,sevenzip,DEFLATE_7ZIP,nthreads);
			lzmaClose7z();
		}else if(zopfli){
			fprintf(stderr,"(zopfli)\n");
			ret=_compress(in,out,zopfli,DEFLATE_ZOPFLI,nthreads);
		}else if(miniz){
			fprintf(stderr,"(miniz)\n");
			ret=_compress(in,out,miniz,DEFLATE_MINIZ,nthreads);
		}else if(slz){
			fprintf(stderr,"(slz)\n");
			ret=_compress(in,out,slz,DEFLATE_SLZ,nthreads);
		}else if(libdeflate){
			fprintf(stderr,"(libdeflate)\n");
			ret=_compress(in,out,libdeflate,DEFLATE_LIBDEFLATE,nthreads);
		}else if(zlibng){
			fprintf(stderr,"(zlibng)\n");
			ret=_compress(in,out,zlibng,DEFLATE_ZLIBNG,nthreads);
		}else if(igzip){
			fprintf(stderr,"(igzip)\n");
			ret=_compress(in,out,igzip,DEFLATE_IGZIP,nthreads);
		}else if(cryptopp){
			fprintf(stderr,"(cryptopp)\n");
			ret=_compress(in,out,cryptopp,DEFLATE_CRYPTOPP,nthreads);
		}else if(kzip){
			fprintf(stderr,"(kzip)\n");
			ret=_compress(in,out,kzip,DEFLATE_KZIP,nthreads);
		}else if(store){
			fprintf(stderr,"(store)\n");
			ret=_compress(in,out,store,DEFLATE_STORE,nthreads);
		}
		fclose(in),fclose(out);
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
