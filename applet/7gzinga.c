// GZinga
// https://tech.ebayinc.com/engineering/gzinga-seekable-and-splittable-gzip/
// https://github.com/eBay/GZinga

// parallel compression is supported.
// parallel decompression is supported.

#ifdef STANDALONE
#include "../compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "../lib/zlibutil.h"
unsigned char buf[BUFLEN];

unsigned long long int read64(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24)|( (unsigned long long int)(x[4]|(x[5]<<8)|(x[6]<<16)|((unsigned int)x[7]<<24)) <<32);
}
unsigned int read32(const void *p);
#if 0
unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24);
}
#endif
unsigned short read16(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8);
}
void write64(void *p, const unsigned long long int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff,
	x[4]=(n>>32)&0xff,x[5]=(n>>40)&0xff,x[6]=(n>>48)&0xff,x[7]=(n>>56)&0xff;
}
void write32(void *p, const unsigned int n);
#if 0
void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}
#endif
void write16(void *p, const unsigned short n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff;
}
void *_memmem(const void *s1, size_t siz1, const void *s2, size_t siz2);
#else
#include "../cielbox.h"
#endif

#include "../lib/lzma.h"
#include "../lib/popt/popt.h"

#if !defined(NOIGZIP)
#include "../lib/isa-l/include/crc.h"
#define fcrc32 crc32_gzip_refl
#else
#define fcrc32 crc32
#endif

#ifdef NOTIMEOFDAY
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifndef BGZF_ST
#include <pthread.h>
#endif

#define GZINGA_HEADER "\x1f\x8b\x08\x10\x00\x00\x00\x00\x00"
#define GZINGA_HEADER_LEN 9

static int _compress(FILE *in, FILE *out, int level, int method, int nthreads){
	const int block_size=100*1024;

	//void* coder=NULL;
	//lzmaCreateCoder(&coder,0x040108,1,level);
	int i=0;
	const int chkpoint_interval=64;
	int chkpoint=chkpoint_interval;
#ifndef BGZF_ST
	pthread_t *threads=(pthread_t*)alloca(sizeof(pthread_t)*nthreads);
#else
	nthreads=1;
#endif

	long long total_size=0;
	int total_block=0;
	for(;/*i<total_block*/;i+=nthreads){
		zlibutil_buffer *zlibbuf_main_thread=NULL;
		int j=0;
		for(;j<nthreads;j++){
			zlibutil_buffer *zlibbuf = zlibutil_buffer_allocate(block_size+(block_size>>1), block_size);
			zlibbuf->sourceLen=fread(zlibbuf->source,1,block_size,in);
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
			}
			zlibbuf->encode = 1;
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
				}
				zlibutil_buffer_free(zlibbuf);
				return 1;
			}
			fwrite("\x1f\x8b\x08\x10\x00\x00\x00\x00\x00\xff\x00",1,11,out);
			fwrite(zlibbuf->dest,1,zlibbuf->destLen,out);
			unsigned int crc=fcrc32(0,zlibbuf->source,zlibbuf->sourceLen);
			write32(buf,crc);
			write32(buf+4,zlibbuf->sourceLen);
			fwrite(buf,1,8,out);
			total_size+=11+zlibbuf->destLen+8;
			total_block=i+j0+1;
			write64(buf+16+8*(i+j0),total_size);
			zlibutil_buffer_free(zlibbuf);
		}
		while((i+nthreads)>=chkpoint){
			fprintf(stderr,"%d\r",i+nthreads);
			chkpoint+=chkpoint_interval;
		}
	}
	fwrite("\x1f\x8b\x08\x10\x00\x00\x00\x00\x00\xff",1,10,out);
	for(int iblk=0;iblk<total_block;iblk++){
		fprintf(out,"%d:%"LLD";",iblk,read64(buf+16+8*iblk));
	}
	fwrite("\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00",1,11,out);
	fprintf(stderr,"%d done.\n",i);
	//lzmaDestroyCoder(&coder);
	return 0;
}

static int _decompress(FILE *in, FILE *out, int nthreads){
	int readlen,i=0;
	//int header_buffer_interval = 32;

	const int chkpoint_interval=256;
	int chkpoint=chkpoint_interval;
#ifndef BGZF_ST
	pthread_t *threads=(pthread_t*)alloca(sizeof(pthread_t)*nthreads);
#else
	nthreads=1;
#endif

	int FOOTER_READ_SIZE=32*1024;
	fseeko(in,0,SEEK_END);
	long long offend=ftello(in);
	fseeko(in,offend-FOOTER_READ_SIZE,SEEK_SET);
	FOOTER_READ_SIZE=fread(cbuf,1,FOOTER_READ_SIZE,in);
	fseeko(in,0,SEEK_SET);
	char *head = _memmem(cbuf,FOOTER_READ_SIZE,GZINGA_HEADER,GZINGA_HEADER_LEN);
	if(!head){
		fprintf(stderr,"not GZinga or corrupted\n");
		return -1;
	}
	FOOTER_READ_SIZE-=head-cbuf;
	for(;;){
		char *next=_memmem(head+GZINGA_HEADER_LEN,FOOTER_READ_SIZE-GZINGA_HEADER_LEN,GZINGA_HEADER,GZINGA_HEADER_LEN);
		if(!next)break;
		FOOTER_READ_SIZE-=next-head;
		head=next;
	}
	//fprintf(stderr,"%s\n",head);
	// head+10 has offsets list; parse them
	int LSTOFFSET=BUFLEN*3/4;
	head+=10;
	long long *lstbegin=(long long*)(buf+LSTOFFSET);
	int total_block=0;
	lstbegin[total_block]=0;
	total_block++;
	head=strtok(head,":");
	for(;;){
		head=strtok(NULL,";");
		if(!head)break;
		lstbegin[total_block]=strtoll(head,NULL,10);
		total_block++;
		head=strtok(NULL,":");
		if(!head)break;
	}

	total_block--;
	for(;i<total_block;i+=nthreads){
		zlibutil_buffer *zlibbuf_main_thread=NULL;
		int j=0;
		for(;i+j<total_block && j<nthreads;j++){
			long long lentoread=lstbegin[i+j+1]-lstbegin[i+j];
			if(lentoread>LSTOFFSET){
				fprintf(stderr,"compressed block does not fit in shared buffer (%"LLD")\n",lentoread);
				return -1;
			}
			//due to original GZinga bug, header size is increasing among blocks. whole block should be read.
			if((readlen=fread(buf,1,lentoread,in))<=0)break;
			if(readlen<lentoread){
				fprintf(stderr,"file truncated\n");
				return -1;		
			}
			int extra_off, extra_len;
			int n=read_gz_header_generic(buf,readlen,&extra_off,&extra_len);
			if(!n){
				fprintf(stderr,"corrupted\n");
				return -1;
			}

			zlibutil_buffer *zlibbuf = zlibutil_buffer_allocate(read32(buf+readlen-4), readlen-n-8);
			memcpy(zlibbuf->source,buf+n,zlibbuf->sourceLen);
			zlibbuf->func = zlibutil_auto_inflate;
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
				fprintf(stderr,"inflate %d\n",zlibbuf->ret);
				zlibutil_buffer_free(zlibbuf);
				return 1;
			}
			fwrite(zlibbuf->dest,1,zlibbuf->destLen,out);
			zlibutil_buffer_free(zlibbuf);
		}
		while((i+nthreads)>=chkpoint){
			fprintf(stderr,"%d\r",i+nthreads);
			chkpoint+=chkpoint_interval;
		}
	}
	fprintf(stderr,"%d done.\n",i);
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int _7gzinga(const int argc, const char **argv){
#endif
	int cmode=0,mode=0;
	int zlib=0,sevenzip=0,zopfli=0,miniz=0,slz=0,libdeflate=0,zlibng=0,igzip=0,cryptopp=0,kzip=0;
	int nthreads=1;
	poptContext optCon;
	int optc;

	struct poptOption optionsTable[] = {
		//{ "longname", "shortname", argInfo,      *arg,       int val, description, argment description}
		{ "stdout", 'c',         POPT_ARG_NONE,            &cmode,      0,       "stdout (currently ignored; always output to stdout)", NULL },
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
		//{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		{ "threads",     '@',         POPT_ARG_INT, &nthreads,    0,       "threads", "threads" },
		{ "decompress", 'd',         POPT_ARG_NONE,            &mode,      0,       "decompress", NULL },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "-cz9 < dec.bin > enc.gzi or -cd enc.gzi > dec.bin");

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
		}
	}

	int level_sum=zlib+sevenzip+zopfli+miniz+slz+libdeflate+zlibng+igzip+cryptopp+kzip;
	if(
		optc<-1 ||
		(!mode&&!zlib&&!sevenzip&&!zopfli&&!miniz&&!slz&&!libdeflate&&!zlibng&&!igzip&&!cryptopp&&!kzip) ||
		(mode&&(zlib||sevenzip||zopfli||miniz||slz||libdeflate||zlibng||igzip||cryptopp||kzip)) ||
		(!mode&&(level_sum==zlib)+(level_sum==sevenzip)+(level_sum==zopfli)+(level_sum==miniz)+(level_sum==slz)+(level_sum==libdeflate)+(level_sum==zlibng)+(level_sum==igzip)+(level_sum==cryptopp)+(level_sum==kzip)!=1)
	){
		poptPrintHelp(optCon, stderr, 0);
		poptFreeContext(optCon);
		if(!lzmaOpen7z())fprintf(stderr,"\nNote: 7-zip is AVAILABLE.\n"),lzmaClose7z();
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
		const char *fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *in=fopen(fname,"rb");
		if(!in){fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);return 2;}
		if(isatty(fileno(stdout)))
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		poptFreeContext(optCon);
		//lzmaOpen7z();
		ret=_decompress(in,stdout,nthreads);
		//lzmaClose7z();
		fclose(in);
	}else{
		if(isatty(fileno(stdin))||isatty(fileno(stdout)))
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		poptFreeContext(optCon);

		fprintf(stderr,"compression level = %d ",level_sum);
		if(zlib){
			fprintf(stderr,"(zlib)\n");
			ret=_compress(stdin,stdout,zlib,DEFLATE_ZLIB,nthreads);
		}else if(sevenzip){
			fprintf(stderr,"(7zip)\n");
			if(lzmaOpen7z()){
				fprintf(stderr,"7-zip is NOT available.\n");
				return -1;
			}
			ret=_compress(stdin,stdout,sevenzip,DEFLATE_7ZIP,nthreads);
			lzmaClose7z();
		}else if(zopfli){
			fprintf(stderr,"(zopfli)\n");
			ret=_compress(stdin,stdout,zopfli,DEFLATE_ZOPFLI,nthreads);
		}else if(miniz){
			fprintf(stderr,"(miniz)\n");
			ret=_compress(stdin,stdout,miniz,DEFLATE_MINIZ,nthreads);
		}else if(slz){
			fprintf(stderr,"(slz)\n");
			ret=_compress(stdin,stdout,slz,DEFLATE_SLZ,nthreads);
		}else if(libdeflate){
			fprintf(stderr,"(libdeflate)\n");
			ret=_compress(stdin,stdout,libdeflate,DEFLATE_LIBDEFLATE,nthreads);
		}else if(zlibng){
			fprintf(stderr,"(zlibng)\n");
			ret=_compress(stdin,stdout,zlibng,DEFLATE_ZLIBNG,nthreads);
		}else if(igzip){
			fprintf(stderr,"(igzip)\n");
			ret=_compress(stdin,stdout,igzip,DEFLATE_IGZIP,nthreads);
		}else if(cryptopp){
			fprintf(stderr,"(cryptopp)\n");
			ret=_compress(stdin,stdout,cryptopp,DEFLATE_CRYPTOPP,nthreads);
		}else if(kzip){
			fprintf(stderr,"(kzip)\n");
			ret=_compress(stdin,stdout,kzip,DEFLATE_KZIP,nthreads);
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
