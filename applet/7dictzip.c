// dictzip
// https://linux.die.net/man/1/dictzip
// "improved dictzip" format conformed.

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

#ifdef NOTIMEOFDAY
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifndef BGZF_ST
#include <pthread.h>
#endif

int inflate_testdecode(z_streamp strm, int flush);

static zlibutil_buffer *zlibutil_buffer_full_flush(zlibutil_buffer *zlibbuf){
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

static int _read_gz_header(unsigned char *data, int size, int *extra_off, int *extra_len, int *block_size, int *total_block){
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
		*extra_len = len;
		if(size < n + len) return 0;
		if(
		   data[n]=='R'&&data[n+1]=='A'&&
		   read16(data+n+2)+4==len&&
		   data[n+4]==0x01&&data[n+5]==0x00&&
		   read16(data+n+8)*2+10==len
		){
			*block_size = read16(data+n+6);
			*total_block = read16(data+n+8);
			// data+n+10 is compsize[short] list
		}else{
			return 0;
		}
		n += len;
	}
	if(flags & ORIG_NAME) while(n < size && data[n++]);
	if(flags & COMMENT) while(n < size && data[n++]);
	if(flags & HEAD_CRC){
		if(n + 2 > size) return 0;
		n += 2;
	}
	return n;
}

static int _compress(FILE *in, FILE *out, int level, int method, int nthreads, int block_size){
	const int max_total_block = 32762; //(0xffff-10)/2
	const long long max_largeblock_size = block_size * max_total_block;
for(;;){
	long long cur_largeblock_size = min(filelengthi64(fileno(in))-ftello(in),max_largeblock_size); // should be <4GB
	long long total_block = (cur_largeblock_size+block_size-1)/block_size;

	fwrite("\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\x03",1,10,out);
	//extra field
	write16(buf,10+2*total_block);
	buf[2]='R';
	buf[3]='A';
	write16(buf+4,6+2*total_block);
	write16(buf+6,1);
	write16(buf+8,block_size);
	write16(buf+10,total_block);
	fwrite(buf,1,12,out);
	long long pos_compsize = ftello(out);
	fwrite(buf,1,total_block*2,out); // this buf can contain garbage.
	// from here buf is used to record compsize.

	unsigned int crc=0;
	const int chkpoint_interval=64;
	int chkpoint=chkpoint_interval;
#ifndef BGZF_ST
	pthread_t *threads=(pthread_t*)alloca(sizeof(pthread_t)*nthreads);
#else
	nthreads=1;
#endif
	long long i=0;
	for(;i<total_block;i+=nthreads){
		zlibutil_buffer *zlibbuf_main_thread=NULL;
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
#ifndef BGZF_ST
				//if(i+j<total_block-1){
					pthread_create(&threads[j],NULL,(void*(*)(void*))zlibutil_buffer_full_flush,zlibbuf);
				//}else{
				//	pthread_create(&threads[j],NULL,(void*(*)(void*))zlibutil_buffer_code,zlibbuf);
				//}
#endif
			}else{
				zlibbuf_main_thread=zlibbuf;
				//if(i+j<total_block-1){
					zlibutil_buffer_full_flush(zlibbuf);
				//}else{
				//	zlibutil_buffer_code(zlibbuf);
				//}
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
				}
				zlibutil_buffer_free(zlibbuf);
				return 1;
			}
			write16(buf+2*(i+j0),zlibbuf->destLen);
			fwrite(zlibbuf->dest,1,zlibbuf->destLen,out);
			zlibutil_buffer_free(zlibbuf);
		}
		while((i+nthreads)>=chkpoint){
			fprintf(stderr,"%"LLD" / %"LLD"\r",i+nthreads,total_block);
			chkpoint+=chkpoint_interval;
		}
	}
	{
		long long pos = ftello(out);
		fseeko(out,pos_compsize,SEEK_SET);
		fwrite(buf,1,2*total_block,out);
		fseeko(out,pos,SEEK_SET);
	}
	buf[0]=0x03;buf[1]=0x00;fwrite(buf,1,2,out); // null deflation to conform normal gzip
	fwrite(&crc,1,4,out);
	fwrite(&cur_largeblock_size,1,4,out);
	fprintf(stderr,"%"LLD" / %"LLD" done.\n",i,total_block);
	if( filelengthi64(fileno(in)) <= ftello(in) )break;
}
	return 0;
}

static int _decompress(FILE *in, FILE *out, int nthreads){
for(;;){
	long long filepos=ftello(in);
	//fprintf(stderr,"%lld %lld\n",filepos,filelengthi64(fileno(in)));
	if(filepos>=filelengthi64(fileno(in)))break;
	fread(buf,1,65536+280,in);
	int offset,len,block_size,total_block;
	int n=_read_gz_header(buf,65536+280,&offset,&len,&block_size,&total_block);
	if(!n){fprintf(stderr,"header is not gzip (possibly corrupted)\n");return 1;}
	memmove(buf,buf+offset+10,2*total_block);
	fseeko(in,filepos+n,SEEK_SET);

	int i=0;

	const int chkpoint_interval=256;
	int chkpoint=chkpoint_interval;
#ifndef BGZF_ST
	pthread_t *threads=(pthread_t*)alloca(sizeof(pthread_t)*nthreads);
#else
	nthreads=1;
#endif
	for(;i<total_block;i+=nthreads){
		zlibutil_buffer *zlibbuf_main_thread=NULL;
		int j=0;
		for(;i+j<total_block && j<nthreads;j++){
			u32 size=read16(buf+(i+j)*2);
			{
				zlibutil_buffer *zlibbuf = zlibutil_buffer_allocate(block_size, size);
				fread(zlibbuf->source,1,size,in);
#if defined(NOIGZIP)
				// libdeflate_inflate cannot be used for Z_FULL_FLUSH stream
				zlibbuf->func = zlib_inflate;
#else
				zlibbuf->func = igzip_inflate;
#endif
				if(j<nthreads-1){
#ifndef BGZF_ST
					pthread_create(&threads[j],NULL,(void*(*)(void*))zlibutil_buffer_code,zlibbuf);
#endif
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
			fprintf(stderr,"%d / %d\r",i+nthreads,total_block);
			chkpoint+=chkpoint_interval;
		}
	}
	fprintf(stderr,"%d / %d done.\n",i,total_block);

	// check next large block
	if(fread(buf,1,12,in)<12)break;
	if(buf[0]==0x03&&buf[1]==0x00&&buf[10]==0x1f&&buf[11]==0x8b){
		fseeko(in,-2,SEEK_CUR);
	}else if(buf[8]==0x1f&&buf[9]==0x8b){
		fseeko(in,-4,SEEK_CUR);
	}
}
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int _7dictzip(const int argc, const char **argv){
#endif
	int cmode=0,mode=0,blk_extreme=0;
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
		{ "extreme", 'X',         POPT_ARG_NONE,            &blk_extreme,      0,       "use 0xff00(65280) bytes block size instead of 58315. Better compression but might not compatible with some softwares.", NULL },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "{-cz9 dec.bin enc.dz} or {-cd enc.dz >dec.bin}");

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

#ifdef NOTIMEOFDAY
	time_t tstart,tend;
	time(&tstart);
#else
	struct timeval tstart,tend;
	gettimeofday(&tstart,NULL);
#endif
	int ret=0;
	if(mode){
		//if(isatty(fileno(stdin))||isatty(fileno(stdout)))
		//	{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}

		const char *fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *in=fopen(fname,"rb");
		if(!in){fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);return 2;}
		poptFreeContext(optCon);

		//lzmaOpen7z();
		ret=_decompress(in,stdout,nthreads);
		//lzmaClose7z();
		fclose(in);
	}else{
		const char *fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *in=fopen(fname,"rb");
		if(!in){fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);return 2;}
		fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);fclose(in);return -1;}
		FILE *out=fopen(fname,"wb");
		if(!out){fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);fclose(in);return 3;}
		poptFreeContext(optCon);

		int block_size = 58315;
		if(blk_extreme)block_size = 0xff00;
		fprintf(stderr,"compression level = %d ",level_sum);
		if(zlib){
			fprintf(stderr,"(zlib)\n");
			ret=_compress(in,out,zlib,DEFLATE_ZLIB,nthreads,block_size);
		}else if(sevenzip){
			fprintf(stderr,"(7zip)\n");
			if(lzmaOpen7z()){
				fprintf(stderr,"7-zip is NOT available.\n");
				return -1;
			}
			ret=_compress(in,out,sevenzip,DEFLATE_7ZIP,nthreads,block_size);
			lzmaClose7z();
		}else if(zopfli){
			fprintf(stderr,"(zopfli)\n");
			ret=_compress(in,out,zopfli,DEFLATE_ZOPFLI,nthreads,block_size);
		}else if(miniz){
			fprintf(stderr,"(miniz)\n");
			ret=_compress(in,out,miniz,DEFLATE_MINIZ,nthreads,block_size);
		}else if(slz){
			fprintf(stderr,"(slz)\n");
			ret=_compress(in,out,slz,DEFLATE_SLZ,nthreads,block_size);
		}else if(libdeflate){
			fprintf(stderr,"(libdeflate)\n");
			ret=_compress(in,out,libdeflate,DEFLATE_LIBDEFLATE,nthreads,block_size);
		}else if(zlibng){
			fprintf(stderr,"(zlibng)\n");
			ret=_compress(in,out,zlibng,DEFLATE_ZLIBNG,nthreads,block_size);
		}else if(igzip){
			fprintf(stderr,"(igzip)\n");
			ret=_compress(in,out,igzip,DEFLATE_IGZIP,nthreads,block_size);
		}
		fclose(out);
		fclose(in);
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
