// RAZF
// based on https://github.com/lh3/samtools/blob/master/razf.c:
// Copyright 2008, Jue Ruan <ruanjue@gmail.com>, Heng Li <lh3@sanger.ac.uk> (2-clause BSDL)

// https://qiita.com/cielavenir/items/7583799d7d2d8d5cce37

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

unsigned long long int read64be(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[7]|(x[6]<<8)|(x[5]<<16)|((unsigned int)x[4]<<24)|( (unsigned long long int)(x[3]|(x[2]<<8)|(x[1]<<16)|((unsigned int)x[0]<<24)) <<32);
}

unsigned int read32be(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[3]|(x[2]<<8)|(x[1]<<16)|((unsigned int)x[0]<<24);
}

unsigned short read16be(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[1]|(x[0]<<8);
}

void write64be(void *p, const unsigned long long int n){
	unsigned char *x=(unsigned char*)p;
	x[7]=n&0xff,x[6]=(n>>8)&0xff,x[5]=(n>>16)&0xff,x[4]=(n>>24)&0xff,
	x[3]=(n>>32)&0xff,x[2]=(n>>40)&0xff,x[1]=(n>>48)&0xff,x[0]=(n>>56)&0xff;
}

void write32be(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[3]=n&0xff,x[2]=(n>>8)&0xff,x[1]=(n>>16)&0xff,x[0]=(n>>24)&0xff;
}

void write16be(void *p, const unsigned short n){
	unsigned char *x=(unsigned char*)p;
	x[1]=n&0xff,x[0]=(n>>8)&0xff;
}

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

//#include "../lib/zlib/zutil.h"
//#include "../lib/zlib/inftrees.h"
//#include "../lib/zlib/inflate.h"

#include <pthread.h>

zlibutil_buffer *zlibutil_buffer_full_flush(zlibutil_buffer *zlibbuf){
	zlibutil_buffer_code(zlibbuf);

	//assert(!zlibbuf->rfc1950);
	z_stream z;

	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if(inflateInit2(&z,-MAX_WBITS) != Z_OK){
		//fprintf(stderr,"inflateInit: %s\n", (z.msg) ? z.msg : "???");
		zlibbuf->ret = -1;
		return zlibbuf;
	}

	z.next_in = zlibbuf->dest;
	z.avail_in = zlibbuf->destLen;
	z.next_out = zlibbuf->source;
	z.avail_out = zlibbuf->sourceLen;

	int bits = inflate_testdecode(&z, Z_FINISH);
	inflateEnd(&z);

	if(bits<3){
		zlibbuf->dest[zlibbuf->destLen++]=0;
	}
	zlibbuf->dest[zlibbuf->destLen++]=0;
	zlibbuf->dest[zlibbuf->destLen++]=0;
	zlibbuf->dest[zlibbuf->destLen++]=0xff;
	zlibbuf->dest[zlibbuf->destLen++]=0xff;

	return zlibbuf;
}

// gzip flag byte
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

static int _read_gz_header(unsigned char *data, int size, int *extra_off, int *extra_len){
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
	if(flags & EXTRA_FIELD){
		if(size < n + 2) return 0;
		len = read16(data+n);
		n += 2;
		*extra_off = n;
		while(len){
			if(n >= size) return 0;
			n ++;
			len --;
		}
		*extra_len = n - (*extra_off);
	}
	if(flags & ORIG_NAME) while(n < size && data[n++]);
	if(flags & COMMENT) while(n < size && data[n++]);
	if(flags & HEAD_CRC){
		if(n + 2 > size) return 0;
		n += 2;
	}
	return n;
}

static int _compress(FILE *in, FILE *out, int level, int method, int nthreads){
	fwrite("\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\x03",1,10,out);
	//extra field
	fwrite("\x07\0RAZF\x01\x80\x00",1,9,out);

	const int block_size=32768;
	long long total_bytes=filelengthi64(fileno(in));
	long long total_block=align2p(block_size,total_bytes)/block_size - 1;
	const int binsize=(1LLU << 32)/block_size;
	long long bins=total_block/binsize;
	memset(buf,0,4+8*(bins+1)+4*total_block);
	write32be(buf,total_block);
	unsigned int crc=0;
	unsigned long long pos=19;

	//void* coder=NULL;
	//lzmaCreateCoder(&coder,0x040108,1,level);
	const int chkpoint_interval=64;
	int chkpoint=chkpoint_interval;
	pthread_t *threads=(pthread_t*)alloca(sizeof(pthread_t)*nthreads);
	long long i=-1;
	for(;i<total_block;i+=nthreads){
		zlibutil_buffer *zlibbuf_main_thread;
		int j=0;
		for(;i+j<total_block && j<nthreads;j++){
			zlibutil_buffer *zlibbuf = zlibutil_buffer_allocate(block_size+(block_size>>1), block_size);
			zlibbuf->sourceLen=fread(zlibbuf->source,1,block_size,in);
			if(zlibbuf->sourceLen==-1)zlibbuf->sourceLen=0;
			if(zlibbuf->sourceLen==0){zlibutil_buffer_free(zlibbuf);break;}
			crc=crc32(crc,zlibbuf->source,zlibbuf->sourceLen);
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
			}
			zlibbuf->encode = 1;
			zlibbuf->level = level;
			if(j<nthreads-1){
				if(i+j<total_block-1){
					pthread_create(&threads[j],NULL,zlibutil_buffer_full_flush,zlibbuf);
				}else{
					pthread_create(&threads[j],NULL,zlibutil_buffer_code,zlibbuf);
				}
			}else{
				zlibbuf_main_thread=zlibbuf;
				if(i+j<total_block-1){
					zlibutil_buffer_full_flush(zlibbuf);
				}else{
					zlibutil_buffer_code(zlibbuf);
				}
			}
		}
		if(!j)break;
		for(int j0=0;j0<j;j0++){
			zlibutil_buffer *zlibbuf;
			if(j0<nthreads-1){
				pthread_join(threads[j0],(void**)&zlibbuf);
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
				}
				zlibutil_buffer_free(zlibbuf);
				return 1;
			}
			if((i+j0)%binsize==0)write64be(buf+4+8*((i+j0)/binsize),pos);
			if((i+j0)>=0)write32be(buf+4+8*(bins+1)+4*(i+j0),pos-read64be(buf+4+8*((i+j)/binsize)));
			fwrite(zlibbuf->dest,1,zlibbuf->destLen,out);
			pos+=zlibbuf->destLen;
			zlibutil_buffer_free(zlibbuf);
		}
		while((i+nthreads)>=chkpoint){
			fprintf(stderr,"%d / %d\r",i+nthreads,total_block);
			chkpoint+=chkpoint_interval;
		}
	}
	fwrite(&crc,1,4,out);
	fwrite(&total_bytes,1,4,out);
	pos+=8;
	unsigned long long block_offset=pos;
	fwrite(buf,1,4+8*(bins+1)+4*total_block,out);
	write64be(buf,total_bytes);
	write64be(buf+8,block_offset);
	fwrite(buf,1,16,out);
	fprintf(stderr,"%"LLD" / %"LLD" done.\n",i,total_block);
	//lzmaDestroyCoder(&coder);
	return 0;
}

static int _decompress(FILE *in, FILE *out, int nthreads){
	fread(buf,1,2048,in);
	int offset,len,block_size;
	int n=_read_gz_header(buf,2048,&offset,&len);
	if(!n){fprintf(stderr,"header is not gzip (possibly corrupted)\n");return 1;}
	if(memcmp(buf+offset,"RAZF",4)){fprintf(stderr,"not RAZF\n");return 1;}
	block_size=read16be(buf+offset+4+1);
	if(fseeko(in,-16,SEEK_END)==-1){fprintf(stderr,"input is not seekable\n");return 1;}
	fread(buf+2048,1,16,in);
	unsigned long long fsize=read64be(buf+2048);
	unsigned long long block_offset=read64be(buf+2048+8);
	fseeko(in,block_offset,SEEK_SET);
	fread(buf,4,1,in);
	int total_block=read32be(buf);
	int binsize=(1LLU << 32)/block_size;
	int bins=total_block/binsize;
	fread(buf,8,bins+1,in);
	fread(buf+8*(bins+1),4,total_block,in);
	write32be(buf+8*(bins+1)+4*total_block,block_offset);
	fseeko(in,offset+len,SEEK_SET);
	int i=-1;
	int counter=offset+len;

	const int chkpoint_interval=256;
	int chkpoint=chkpoint_interval;
	pthread_t *threads=(pthread_t*)alloca(sizeof(pthread_t)*nthreads);
	for(;i<total_block;i+=nthreads){
		zlibutil_buffer *zlibbuf_main_thread;
		int j=0;
		for(;i+j<total_block && j<nthreads;j++){
			u32 index=i+j<0 ? offset+len : read32be(buf+8*(bins+1)+4*(i+j))+read64be(buf+((i+j)/binsize)*8);
			u32 pos=index<<0;
			if(pos>counter)fread(buf+BUFLEN-(pos-counter),1,pos-counter,in); //discard

			u32 size=read32be(buf+8*(bins+1)+4*(i+j+1))+read64be(buf+(i+j+1)/binsize*8) - index;
			//if(plain){
			//}else
			{
				zlibutil_buffer *zlibbuf = zlibutil_buffer_allocate(block_size, size);
				fread(zlibbuf->source,1,size,in);counter+=size;
#if defined(NOIGZIP)
				// libdeflate_inflate cannot be used for Z_FULL_FLUSH stream
				zlibbuf->func = zlib_inflate;
#else
				zlibbuf->func = igzip_inflate;
#endif
				if(j<nthreads-1){
					pthread_create(&threads[j],NULL,zlibutil_buffer_code,zlibbuf);
				}else{
					zlibbuf_main_thread=zlibbuf;
					zlibutil_buffer_code(zlibbuf);
				}
			}
		}
		if(!j)break;
		for(int j0=0;j0<j;j0++){
			zlibutil_buffer *zlibbuf;
			if(j0<nthreads-1){
				pthread_join(threads[j0],(void**)&zlibbuf);
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
			fprintf(stderr,"%d / %d\r",i+nthreads,total_block);
			chkpoint+=chkpoint_interval;
		}
	}
	fprintf(stderr,"%d / %d done.\n",i,total_block);
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int _7razf(const int argc, const char **argv){
#endif
	int cmode=0,mode=0;
	int zlib=0,sevenzip=0,zopfli=0,miniz=0,slz=0,libdeflate=0,zlibng=0,igzip=0;
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
		{ "igzip",     'i',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'i',       "1-4 (default 1) igzip (1 becomes igzip internal level 0, 2 becomes 1, ...)", "level" },
		{ "zopfli",     'Z',         POPT_ARG_INT, &zopfli,    0,       "zopfli", "numiterations" },
		//{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		{ "threads",     '@',         POPT_ARG_INT, &nthreads,    0,       "threads", "threads" },
		{ "decompress", 'd',         POPT_ARG_NONE,            &mode,      0,       "decompress", NULL },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "{-cz9 dec.bin >enc.raz} or {-cd enc.raz >dec.bin}");

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
			case 'i':{
				char *arg=poptGetOptArg(optCon);
				if(arg)igzip=strtol(arg,NULL,10),free(arg);
				else igzip=1;
				break;
			}
		}
	}

	int level_sum=zlib+sevenzip+zopfli+miniz+slz+libdeflate+zlibng+igzip;
	if(
		optc<-1 ||
		(!mode&&!zlib&&!sevenzip&&!zopfli&&!miniz&&!slz&&!libdeflate&&!zlibng&&!igzip) ||
		(mode&&(zlib||sevenzip||zopfli||miniz||slz||libdeflate||zlibng||igzip)) ||
		(!mode&&(level_sum==zlib)+(level_sum==sevenzip)+(level_sum==zopfli)+(level_sum==miniz)+(level_sum==slz)+(level_sum==libdeflate)+(level_sum==zlibng)+(level_sum==igzip)!=1)
	){
		poptPrintHelp(optCon, stderr, 0);
		poptFreeContext(optCon);
		if(!lzmaOpen7z())fprintf(stderr,"\nNote: 7-zip is AVAILABLE.\n"),lzmaClose7z();
		else fprintf(stderr,"\nNote: 7-zip is NOT available.\n");
		return 1;
	}
	if(nthreads<1)nthreads=1;

	if(mode){
		//if(isatty(fileno(stdin))||isatty(fileno(stdout)))
		//	{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}

		const char *fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *in=fopen(fname,"rb");
		if(!in){fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);return 2;}
		poptFreeContext(optCon);

		//lzmaOpen7z();
		int ret=_decompress(in,stdout,nthreads);
		//lzmaClose7z();
		fclose(in);
		return ret;
	}else{
		const char *fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *in=fopen(fname,"rb");
		if(!in){fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);return 2;}
		poptFreeContext(optCon);

		fprintf(stderr,"compression level = %d ",level_sum);
		int ret=0;
		if(zlib){
			fprintf(stderr,"(zlib)\n");
			ret=_compress(in,stdout,zlib,DEFLATE_ZLIB,nthreads);
		}else if(sevenzip){
			fprintf(stderr,"(7zip)\n");
			if(lzmaOpen7z()){
				fprintf(stderr,"7-zip is NOT available.\n");
				return -1;
			}
			ret=_compress(in,stdout,sevenzip,DEFLATE_7ZIP,nthreads);
			lzmaClose7z();
		}else if(zopfli){
			fprintf(stderr,"(zopfli)\n");
			ret=_compress(in,stdout,zopfli,DEFLATE_ZOPFLI,nthreads);
		}else if(miniz){
			fprintf(stderr,"(miniz)\n");
			ret=_compress(in,stdout,miniz,DEFLATE_MINIZ,nthreads);
		}else if(slz){
			fprintf(stderr,"(slz)\n");
			ret=_compress(in,stdout,slz,DEFLATE_SLZ,nthreads);
		}else if(libdeflate){
			fprintf(stderr,"(libdeflate)\n");
			ret=_compress(in,stdout,libdeflate,DEFLATE_LIBDEFLATE,nthreads);
		}else if(zlibng){
			fprintf(stderr,"(zlibng)\n");
			ret=_compress(in,stdout,zlibng,DEFLATE_ZLIBNG,nthreads);
		}else if(igzip){
			fprintf(stderr,"(igzip)\n");
			ret=_compress(in,stdout,igzip,DEFLATE_IGZIP,nthreads);
		}
		fclose(in);
		return ret;
	}
}
