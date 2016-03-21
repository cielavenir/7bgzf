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
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24);
}
unsigned short read16(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8);
}
void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}
void write16(void *p, const unsigned short n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff;
}

#else
#include "../cielbox.h"
#endif

#include "../lib/zopfli/deflate.h"
#include "../lib/lzma.h"
#include "../lib/popt/popt.h"

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
	if(flags & EXTRA_FIELD){
		if(size < n + 2) return 0;
		len = read16(data+n);
		n += 2;
		*extra_off = n;
		if(len!=6 || memcmp(data+n,"BC\x02\x00",4))return 0;
		n += 4;
		*block_len = read16(data+n);
		n += 2;
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

static int _compress(FILE *in, FILE *out, int level){
	const int block_size=65536;

#ifndef FEOS
	ZopfliOptions options;
if(level>9){
	ZopfliInitOptions(&options);
	options.numiterations = level/10;
}
#endif
	void* coder=NULL;
	lzmaCreateCoder(&coder,0x040108,1,level);
	int i=0,offset=0;
	for(;/*i<total_block*/;i++){
		size_t readlen=fread(__decompbuf+offset,1,block_size-offset,in);
		if(readlen==-1)break;
		readlen+=offset;
		if(readlen==0)break;
		size_t blksize=readlen;
		for(;;){
			size_t compsize=0;
			unsigned char* COMPBUF = __compbuf;
#ifndef FEOS
			if(level>9){
				size_t __compsize=0;
				unsigned char bp = 0;
				COMPBUF=NULL;
				ZopfliDeflate(&options, 2 /* Dynamic block */, 1, __decompbuf, (size_t)blksize, &bp, &COMPBUF, &__compsize);
				compsize=__compsize;
			}else
#endif
			if(coder){
				compsize=COMPBUFLEN;
				int r=lzmaCodeOneshot(coder,__decompbuf,blksize,COMPBUF,&compsize);
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
				z.avail_in = blksize;
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

			size_t compsize_record=18+compsize+8-1;
			if(compsize_record>65535){
				blksize-=1024;
#ifndef FEOS
				if(level>9)free(COMPBUF);
#endif
				continue;
			}
			fwrite("\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\xff",1,10,out);
			//extra field
			fwrite("\x06\0BC\x02\x00",1,6,out);
			write16(buf,compsize_record);
			fwrite(buf,1,2,out);
			fwrite(COMPBUF,1,compsize,out);
			unsigned int crc=crc32(0,__decompbuf,blksize);
			write32(buf,crc);
			write32(buf+4,blksize);
			fwrite(buf,1,8,out);
			offset=readlen-blksize;
			memmove(__decompbuf,__decompbuf+readlen-offset,offset);
#ifndef FEOS
			if(level>9)free(COMPBUF);
#endif
			break;
		}
		if((i+1)%64==0)fprintf(stderr,"%d\r",i+1);
	}
	fprintf(stderr,"%d done.\n",i);
	lzmaDestroyCoder(&coder);
	return 0;
}

static int _decompress(FILE *in, FILE *out){
	const int block_size=65536;
	int readlen,i=0;
	long long filepos=0,rawpos=0;

	//void* coder=NULL;
	//lzmaCreateCoder(&coder,0x040108,0,0);
	for(;(readlen=fread(buf,1,1024,in))>0;i++){ //there shouldn't be two blocks within 1024bytes...
		int extra_off, extra_len, block_len;
		int n=_read_gz_header(buf,1024,&extra_off,&extra_len,&block_len);
		if(!n){
			fprintf(stderr,"not BGZF or corrupted\n");
			return -1;
		}
		block_len+=1;
		memmove(buf,buf+n,readlen-n); //ignore header
		fread(buf+readlen-n,1,block_len-readlen,in);
		{
			size_t decompsize=block_size;
//if(coder){
//		int r=lzmaCodeOneshot(coder,buf,block_len-n,__decompbuf,&decompsize);
//		if(r){
//			fprintf(stderr,"NDeflate::CCoder::Code %d\n",r);
//			return 1;
//		}
//}else
{
			z_stream z;
			int status=Z_OK;

			z.zalloc = Z_NULL;
			z.zfree = Z_NULL;
			z.opaque = Z_NULL;

			if(inflateInit2(&z,-MAX_WBITS) != Z_OK){
				fprintf(stderr,"inflateInit: %s\n", (z.msg) ? z.msg : "???");
				return 1;
			}

			z.next_in = buf;
			z.avail_in = block_len-n;
			z.next_out = __decompbuf;
			z.avail_out = decompsize;

		for(;z.avail_out && status != Z_STREAM_END;){
			status = inflate(&z, Z_BLOCK);
			if(status==Z_BUF_ERROR)break;
			if(status != Z_STREAM_END && status != Z_OK){
				fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
				return 10;
			}
		}

			if(inflateEnd(&z) != Z_OK){
				fprintf(stderr,"inflateEnd: %s\n", (z.msg) ? z.msg : "???");
				return 2;
			}
			decompsize=block_size-z.avail_out;

			//fprintf(stderr,"%016llx %016llx\n",filepos,rawpos);
			//rawpos+=decompsize;
			//filepos+=block_len;
}
			fwrite(__decompbuf,1,decompsize,out);
		}
		if((i+1)%256==0)fprintf(stderr,"%d\r",i+1);
	}
	fprintf(stderr,"%d done.\n",i);
	//lzmaDestroyCoder(&coder);
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
		{ "decompress", 'd',         0,            &mode,      0,       "decompress", NULL },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "-c2/-z1 < dec.bin > enc.bgz or -d < enc.bgz > dec.bin");

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
		if(isatty(fileno(stdin))||isatty(fileno(stdout)))
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		poptFreeContext(optCon);
		//lzmaOpen7z();
		int ret=_decompress(stdin,stdout);
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
