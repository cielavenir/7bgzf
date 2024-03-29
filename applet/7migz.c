// MiGz
// https://engineering.linkedin.com/blog/2019/02/migz-for-compression-and-decompression
// https://github.com/linkedin/migz

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

#else
#include "../cielbox.h"
#endif

#if !defined(NOIGZIP)
#include "../lib/isa-l/include/crc.h"
#define fcrc32 crc32_gzip_refl
#else
#define fcrc32 crc32
#endif

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

// gzip flag byte
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

static int _read_gz_header(unsigned char *data, int size, int *extra_off, int *extra_len, int *block_len){
	int method, flags, n, len;
	if(size < 2) return 0;
	if(data[0] != 0x1f || data[1] != 0x8b) return 0;
	if(size < 4) return 0;
	method = data[2];
	flags  = data[3];
	if(method != Z_DEFLATED || (flags & RESERVED)) return 0;
	n = 4 + 6; // Skip 6 bytes
	*extra_off = n + 2;
	*extra_len = 0;
	*block_len = 0;
	if(flags & EXTRA_FIELD){
		if(size < n + 2) return 0;
		len = read16(data+n);
		n += 2;
		*extra_off = n;
		*extra_len = len;
		if(size < n + len) return 0;
		n += len;
	}
	if(flags & ORIG_NAME) while(n < size && data[n++]);
	if(flags & COMMENT) while(n < size && data[n++]);
	if(flags & HEAD_CRC){
		if(n + 2 > size) return 0;
		n += 2;
	}
	if(*extra_len==6 && !memcmp(data+*extra_off,"BC\x02\x00",4)){
		// BGZF: 0600 BC 0200 hdrftrcmpsize-1(2)
		*block_len = read16(data+*extra_off+4)+1;
	}else if(*extra_len==8 && !memcmp(data+*extra_off,"MZ\x04\x00",4)){
		// MiGz: 0800 MZ 0400 cmpsize(4)
		*block_len = read32(data+*extra_off+4)+n+8; // adding header and footer size
	}else if(*extra_len==20 && !memcmp(data+*extra_off,"IG\x10\x00",4)){
		// https://github.com/vinlyx/mgzip [v1]
		// 1400 IG 1000 hdrftrcmpsize(8) decompsize(8)
		// hdrftrcmpsize should not exceed 4GB. this is block size, not file size.
		*block_len = read64(data+*extra_off+4);
	}else if(*extra_len==8 && !memcmp(data+*extra_off,"IG\x04\x00",4)){
		// https://github.com/vinlyx/mgzip [v2]
		// 0800 IG 0400 hdrftrcmpsize(4)
		*block_len = read32(data+*extra_off+4);
	}else if(*extra_len==4 && data[*extra_off+3]==0x7d){
		// https://github.com/jerodsanto/mgzip
		// 0400 hdrftrcmpsize(3) 7d
		*block_len = read32(data+*extra_off)&0xffffff;
	}else{
		return 0;
	}
	return n;
}

static int _compress(FILE *in, FILE *out, int level, int method, int nthreads, int bsize){
	const int block_size=bsize*1024;

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
			}else if(method==DEFLATE_STORE){
				zlibbuf->func = store_deflate;
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
				}else if(method==DEFLATE_STORE){
					fprintf(stderr,"store_deflate %d\n",zlibbuf->ret);
				}
				zlibutil_buffer_free(zlibbuf);
				return 1;
			}
			size_t compsize_record=zlibbuf->destLen;
			fwrite("\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\xff",1,10,out);
			//extra field
			fwrite("\x08\0MZ\x04\x00",1,6,out);
			write32(buf,compsize_record);
			fwrite(buf,1,4,out);
			fwrite(zlibbuf->dest,1,zlibbuf->destLen,out);
			unsigned int crc=fcrc32(0,zlibbuf->source,zlibbuf->sourceLen);
			write32(buf,crc);
			write32(buf+4,zlibbuf->sourceLen);
			fwrite(buf,1,8,out);
			zlibutil_buffer_free(zlibbuf);
		}
		while((i+nthreads)>=chkpoint){
			fprintf(stderr,"%d\r",i+nthreads);
			chkpoint+=chkpoint_interval;
		}
	}
	fprintf(stderr,"%d done.\n",i);
	//lzmaDestroyCoder(&coder);
	return 0;
}

static int _decompress(FILE *in, FILE *out, int nthreads){
	int readlen,i=0;
	const int header_buffer_interval = 128;

	const int chkpoint_interval=256;
	int chkpoint=chkpoint_interval;
#ifndef BGZF_ST
	pthread_t *threads=(pthread_t*)alloca(sizeof(pthread_t)*nthreads);
#else
	nthreads=1;
#endif
	for(;;i+=nthreads){
		zlibutil_buffer *zlibbuf_main_thread=NULL;
		int j=0;
		for(;j<nthreads;j++){
			if((readlen=fread(buf,1,header_buffer_interval,in))<=0)break;
			int extra_off, extra_len, block_len=0;
			int n=_read_gz_header(buf,readlen,&extra_off,&extra_len,&block_len);
			if(!n){
				fprintf(stderr,"not MiGz or corrupted\n");
				return -1;
			}
			if(block_len>BUFLEN){
				fprintf(stderr,"compressed block does not fit in shared buffer (%d)\n",block_len);
				return -1;
			}
			memmove(buf,buf+n,readlen-n); //ignore header
			if(block_len-readlen<0){
				fprintf(stderr,"sorry, header buffer interval (%d) is too big for this file block (%d).\n",header_buffer_interval,block_len);
				return -1;
			}
			fread(buf+readlen-n,1,block_len-readlen,in);

			zlibutil_buffer *zlibbuf = zlibutil_buffer_allocate(read32(buf+block_len-n-4), block_len-n);
			memcpy(zlibbuf->source,buf,zlibbuf->sourceLen);
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
int _7migz(const int argc, const char **argv){
#endif
	int cmode=0,mode=0;
	int zlib=0,sevenzip=0,zopfli=0,miniz=0,slz=0,libdeflate=0,zlibng=0,igzip=0,cryptopp=0,kzip=0,store=0;
	int nthreads=1;
	int bsize=512;
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
		{ "store",     'T',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'T',       "1-1 (default 1) store", "level" },
		//{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		{ "threads",     '@',         POPT_ARG_INT, &nthreads,    0,       "threads", "threads" },
		{ "decompress", 'd',         POPT_ARG_NONE,            &mode,      0,       "decompress", NULL },
		{ "bsize",     'b',         POPT_ARG_INT, &bsize,    0,       "bsize in 1024b unit (512)", "bsize" },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "-cz9 < dec.bin > enc.mgz or -cd < enc.mgz > dec.bin");

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
		if(isatty(fileno(stdin))||isatty(fileno(stdout)))
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		poptFreeContext(optCon);

		fprintf(stderr,"compression level = %d ",level_sum);
		if(zlib){
			fprintf(stderr,"(zlib)\n");
			ret=_compress(stdin,stdout,zlib,DEFLATE_ZLIB,nthreads,bsize);
		}else if(sevenzip){
			fprintf(stderr,"(7zip)\n");
			if(lzmaOpen7z()){
				fprintf(stderr,"7-zip is NOT available.\n");
				return -1;
			}
			ret=_compress(stdin,stdout,sevenzip,DEFLATE_7ZIP,nthreads,bsize);
			lzmaClose7z();
		}else if(zopfli){
			fprintf(stderr,"(zopfli)\n");
			ret=_compress(stdin,stdout,zopfli,DEFLATE_ZOPFLI,nthreads,bsize);
		}else if(miniz){
			fprintf(stderr,"(miniz)\n");
			ret=_compress(stdin,stdout,miniz,DEFLATE_MINIZ,nthreads,bsize);
		}else if(slz){
			fprintf(stderr,"(slz)\n");
			ret=_compress(stdin,stdout,slz,DEFLATE_SLZ,nthreads,bsize);
		}else if(libdeflate){
			fprintf(stderr,"(libdeflate)\n");
			ret=_compress(stdin,stdout,libdeflate,DEFLATE_LIBDEFLATE,nthreads,bsize);
		}else if(zlibng){
			fprintf(stderr,"(zlibng)\n");
			ret=_compress(stdin,stdout,zlibng,DEFLATE_ZLIBNG,nthreads,bsize);
		}else if(igzip){
			fprintf(stderr,"(igzip)\n");
			ret=_compress(stdin,stdout,igzip,DEFLATE_IGZIP,nthreads,bsize);
		}else if(cryptopp){
			fprintf(stderr,"(cryptopp)\n");
			ret=_compress(stdin,stdout,cryptopp,DEFLATE_CRYPTOPP,nthreads,bsize);
		}else if(kzip){
			fprintf(stderr,"(kzip)\n");
			ret=_compress(stdin,stdout,kzip,DEFLATE_KZIP,nthreads,bsize);
		}else if(store){
			fprintf(stderr,"(store)\n");
			ret=_compress(stdin,stdout,store,DEFLATE_STORE,nthreads,bsize);
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
