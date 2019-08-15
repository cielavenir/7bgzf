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
unsigned short read16(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8);
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

#else
#include "../cielbox.h"
#endif

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

static int _compress(FILE *in, FILE *out, int level, int method){
	const int block_size=65536;

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
			size_t compsize=COMPBUFLEN;
			if(method==DEFLATE_ZLIB){
				int r=zlib_deflate(__compbuf,&compsize,__decompbuf,blksize,level);
				if(r){
					fprintf(stderr,"deflate %d\n",r);
					return 1;
				}
			}
			if(method==DEFLATE_7ZIP){
				if(coder){
					int r=lzmaCodeOneshot(coder,__decompbuf,blksize,__compbuf,&compsize);
					if(r){
						fprintf(stderr,"NDeflate::CCoder::Code %d\n",r);
						return 1;
					}
				}
			}
			if(method==DEFLATE_ZOPFLI){
				int r=zopfli_deflate(__compbuf,&compsize,__decompbuf,blksize,level);
				if(r){
					fprintf(stderr,"zopfli_deflate %d\n",r);
					return 1;
				}
			}
			if(method==DEFLATE_MINIZ){
				int r=miniz_deflate(__compbuf,&compsize,__decompbuf,blksize,level);
				if(r){
					fprintf(stderr,"miniz_deflate %d\n",r);
					return 1;
				}
			}
			if(method==DEFLATE_SLZ){
				int r=slz_deflate(__compbuf,&compsize,__decompbuf,blksize,level);
				if(r){
					fprintf(stderr,"slz_deflate %d\n",r);
					return 1;
				}
			}
			if(method==DEFLATE_LIBDEFLATE){
				int r=libdeflate_deflate(__compbuf,&compsize,__decompbuf,blksize,level);
				if(r){
					fprintf(stderr,"libdeflate_deflate %d\n",r);
					return 1;
				}
			}

			size_t compsize_record=18+compsize+8-1;
			if(compsize_record>65535){
				blksize-=1024;
				continue;
			}
			fwrite("\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\xff",1,10,out);
			//extra field
			fwrite("\x06\0BC\x02\x00",1,6,out);
			write16(buf,compsize_record);
			fwrite(buf,1,2,out);
			fwrite(__compbuf,1,compsize,out);
			unsigned int crc=crc32(0,__decompbuf,blksize);
			write32(buf,crc);
			write32(buf+4,blksize);
			fwrite(buf,1,8,out);
			offset=readlen-blksize;
			memmove(__decompbuf,__decompbuf+readlen-offset,offset);
			break;
		}
		if((i+1)%64==0)fprintf(stderr,"%d\r",i+1);
	}
	fwrite(
		"\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\xff" // header         (10)
		"\x06\0BC\x02\x00"                         // extra header   (6)
		"\x1b\x00"                                 // size(28-1)     (2)
		"\x03\x00"                                 // null deflation (2)
		"\x00\x00\x00\x00\x00\x00\x00\x00",        // footer         (8)
		1,28,out);
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

			int r=zlib_inflate(__decompbuf,&decompsize,buf,block_len-n);
			if(r){
				fprintf(stderr,"inflate %d\n",r);
				return 1;
			}

			//fprintf(stderr,"%016llx %016llx\n",filepos,rawpos);
			//rawpos+=decompsize;
			//filepos+=block_len;

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
	initstdio();
#else
int _7bgzf(const int argc, const char **argv){
#endif
	int cmode=0,mode=0;
	int zlib=0,sevenzip=0,zopfli=0,miniz=0,slz=0,libdeflate=0;
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
		{ "zopfli",     'Z',         POPT_ARG_INT, &zopfli,    0,       "zopfli", "numiterations" },
		//{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		{ "decompress", 'd',         POPT_ARG_NONE,            &mode,      0,       "decompress", NULL },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "-cz9 < dec.bin > enc.bgz or -cd < enc.bgz > dec.bin");

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
		}
	}

	int level_sum=zlib+sevenzip+zopfli+miniz+slz+libdeflate;
	if(
		optc<-1 ||
		(!mode&&!zlib&&!sevenzip&&!zopfli&&!miniz&&!slz&&!libdeflate) ||
		(mode&&(zlib||sevenzip||zopfli||miniz||slz||libdeflate)) ||
		(!mode&&(level_sum==zlib)+(level_sum==sevenzip)+(level_sum==zopfli)+(level_sum==miniz)+(level_sum==slz)+(level_sum==libdeflate)!=1)
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

		fprintf(stderr,"compression level = %d ",level_sum);
		int ret=0;
		if(zlib){
			fprintf(stderr,"(zlib)\n");
			ret=_compress(stdin,stdout,zlib,DEFLATE_ZLIB);
		}else if(sevenzip){
			fprintf(stderr,"(7zip)\n");
			if(lzmaOpen7z()){
				fprintf(stderr,"7-zip is NOT available.\n");
				return -1;
			}
			ret=_compress(stdin,stdout,sevenzip,DEFLATE_7ZIP);
			lzmaClose7z();
		}else if(zopfli){
			fprintf(stderr,"(zopfli)\n");
			ret=_compress(stdin,stdout,zopfli,DEFLATE_ZOPFLI);
		}else if(miniz){
			fprintf(stderr,"(miniz)\n");
			ret=_compress(stdin,stdout,miniz,DEFLATE_MINIZ);
		}else if(slz){
			fprintf(stderr,"(slz)\n");
			ret=_compress(stdin,stdout,slz,DEFLATE_SLZ);
		}else if(libdeflate){
			fprintf(stderr,"(libdeflate)\n");
			ret=_compress(stdin,stdout,libdeflate,DEFLATE_LIBDEFLATE);
		}
		return ret;
	}
}
