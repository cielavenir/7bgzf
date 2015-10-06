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

#define DECOMPBUFLEN 32768
#define COMPBUFLEN   (DECOMPBUFLEN|(DECOMPBUFLEN>>1))
unsigned char __compbuf[COMPBUFLEN],__decompbuf[DECOMPBUFLEN];

unsigned long long int read64(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|(x[3]<<24)|( (unsigned long long int)(x[4]|(x[5]<<8)|(x[6]<<16)|(x[7]<<24)) <<32);
}

unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|(x[3]<<24);
}

unsigned short read16(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8);
}

void write64(void *p, const unsigned long long int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff,
	x[4]=(n>>32)&0xff,x[5]=(n>>40)&0xff,x[6]=(n>>48)&0xff,x[7]=(n>>56)&0xff;
}

void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}

void write16(void *p, const unsigned short n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff;
}

unsigned long long int read64be(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[7]|(x[6]<<8)|(x[5]<<16)|(x[4]<<24)|( (unsigned long long int)(x[3]|(x[2]<<8)|(x[1]<<16)|(x[0]<<24)) <<32);
}

unsigned int read32be(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[3]|(x[2]<<8)|(x[1]<<16)|(x[0]<<24);
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

#if defined(WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
int filelength(int fd){ //constant phrase
	struct stat st;
	fstat(fd,&st);
	return st.st_size;
}
#endif

#else
#include "../cielbox.h"
#endif

#include "../lib/zopfli/deflate.h"
#include "../lib/lzma.h"
#include "../lib/popt/popt.h"

// https://github.com/lh3/samtools/blob/master/razf.c

// gzip flag byte
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

int _read_gz_header(unsigned char *data, int size, int *extra_off, int *extra_len){
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
		len = ((int)data[n + 1] << 8) | data[n];
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

static int _compress(FILE *in, FILE *out, int level){
	fwrite("\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\x03",1,10,out);
	//extra field
	fwrite("\x07\0RAZF\x01\x80\x00",1,9,out);

	int block_size=32768;
	int total_bytes=filelength(fileno(in));
	int total_block=align2p(block_size,total_bytes)/block_size - 1;
	int binsize=(1LLU << 32)/block_size;
	int bins=total_block/binsize;
	memset(buf,0,4+8*(bins+1)+4*total_block);
	write32be(buf,total_block);
	unsigned int crc=0;
	unsigned long long pos=19;

#ifndef FEOS
	ZopfliOptions options;
if(level>9){
	ZopfliInitOptions(&options);
	options.numiterations = level/10;
}
#endif
	void* coder=NULL;
	lzmaCreateCoder(&coder,0x040108,1,level);
	int i=-1;
	for(;i<total_block;i++){
		if(i%binsize==0)write64be(buf+4+8*(i/binsize),pos);
		if(i>=0)write32be(buf+4+8*(bins+1)+4*i,pos-read64be(buf+4+8*(i/binsize)));

		size_t readlen=fread(__decompbuf,1,block_size,in);
		crc=crc32(crc,__decompbuf,readlen);
		size_t compsize=0;
		unsigned char* COMPBUF = __compbuf;
#ifndef FEOS
if(level>9){
		size_t __compsize=0;
		unsigned char bp = 0;
		COMPBUF=NULL;
		ZopfliDeflate(&options, 2 /* Dynamic block */, 1, __decompbuf, (size_t)readlen, &bp, &COMPBUF, &__compsize);
		compsize=__compsize;
}else
#endif
if(coder){
		compsize=COMPBUFLEN;
		int r=lzmaCodeOneshot(coder,__decompbuf,readlen,COMPBUF,&compsize);
		if(r){
			fprintf(stderr,"NDeflate::CCoder::Code %d\n",r);
			return 1;
		}
}else{
		z_stream z;
		int status;

		z.zalloc = Z_NULL;
		z.zfree = Z_NULL;
		z.opaque = Z_NULL;

		if(deflateInit2(&z, level , Z_DEFLATED, -MAX_WBITS, level, Z_DEFAULT_STRATEGY) != Z_OK){
			fprintf(stderr,"deflateInit: %s\n", (z.msg) ? z.msg : "???");
			return 1;
		}

		z.next_in = __decompbuf;
		z.avail_in = readlen;
		z.next_out = __compbuf;
		z.avail_out = COMPBUFLEN;

		status = deflate(&z, Z_FINISH);
		if(status != Z_STREAM_END && status != Z_OK){
			fprintf(stderr,"deflate: %s\n", (z.msg) ? z.msg : "???");
			return 10;
		}
		compsize=COMPBUFLEN-z.avail_out;

		if(deflateEnd(&z) != Z_OK){
			//fprintf(stderr,"deflateEnd: %s\n", (z.msg) ? z.msg : "???");
			//return 2;
		}
}

		if(i<total_block-1){
			z_stream z;

			z.zalloc = Z_NULL;
			z.zfree = Z_NULL;
			z.opaque = Z_NULL;

			if(inflateInit2(&z,-MAX_WBITS) != Z_OK){
				fprintf(stderr,"inflateInit: %s\n", (z.msg) ? z.msg : "???");
				return 1;
			}

			z.next_in = COMPBUF;
			z.avail_in = compsize;
			z.next_out = __decompbuf;
			z.avail_out = block_size;

			int bits = inflate_testdecode(&z, Z_FINISH);
			inflateEnd(&z);

			if(bits<3){
				COMPBUF[compsize++]=0;
			}
			COMPBUF[compsize++]=0;
			COMPBUF[compsize++]=0;
			COMPBUF[compsize++]=0xff;
			COMPBUF[compsize++]=0xff;
		}

		fwrite(COMPBUF,1,compsize,out);
		pos+=compsize;
		
#ifndef FEOS
if(level>9)free(COMPBUF);
#endif
		if((i+1)%256==0)fprintf(stderr,"%d / %d\r",i+1,total_block);
	}
	fwrite(&crc,1,4,out);
	fwrite(&total_bytes,1,4,out);
	pos+=8;
	unsigned long long block_offset=pos;
	fwrite(buf,1,4+8*(bins+1)+4*total_block,out);
	write64be(buf,total_bytes);
	write64be(buf+8,block_offset);
	fwrite(buf,1,16,out);
	fprintf(stderr,"%d / %d done.\n",i,total_block);
	lzmaDestroyCoder(&coder);
	return 0;
}

static int _decompress(FILE *in, FILE *out){
	fread(buf,1,2048,in);
	int offset,len,block_size;
	int n=_read_gz_header(buf,2048,&offset,&len);
	if(!n){fprintf(stderr,"header is not gzip (possibly corrupted)\n");return 1;}
	if(memcmp(buf+offset,"RAZF",4)){fprintf(stderr,"not RAZF\n");return 1;}
	block_size=read16be(buf+offset+4+1);
	if(fseek(in,-16,SEEK_END)==-1){fprintf(stderr,"input is not seekable\n");return 1;}
	fread(buf+2048,1,16,in);
	unsigned long long fsize=read64be(buf+2048);
	unsigned long long block_offset=read64be(buf+2048+8);
	fseek(in,block_offset,SEEK_SET);
	fread(buf,4,1,in);
	int total_block=read32be(buf);
	int binsize=(1LLU << 32)/block_size;
	int bins=total_block/binsize;
	fread(buf,8,bins+1,in);
	fread(buf+8*(bins+1),4,total_block,in);
	write32be(buf+8*(bins+1)+4*total_block,block_offset);
	fseek(in,offset+len,SEEK_SET);
	int i=-1;
	int counter=offset+len;

	//void* coder=NULL;
	//lzmaCreateCoder(&coder,0x040108,0,0);
	for(;i<total_block;i++){
		u32 index=i<0 ? offset+len : read32be(buf+8*(bins+1)+4*i)+read64be(buf+(i/binsize)*8);
		u32 pos=index<<0;
		if(pos>counter)fread(buf+BUFLEN-(pos-counter),1,pos-counter,in); //discard

		u32 size=read32be(buf+8*(bins+1)+4*(i+1))+read64be(buf+(i+1)/binsize*8) - index;
		//if(plain){
		//	fread(__decompbuf,1,size,in);counter+=size;
		//	fwrite(__decompbuf,1,size,out);
		//}else
		{
			fread(__compbuf,1,size,in);counter+=size;
			size_t decompsize=block_size;
//if(coder){
//		int r=lzmaCodeOneshot(coder,__compbuf,size,__decompbuf,&decompsize);
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

			z.next_in = __compbuf;
			z.avail_in = size;
			z.next_out = __decompbuf;
			z.avail_out = block_size;

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
#else
int _7razf(const int argc, const char **argv){
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
	poptSetOtherOptionHelp(optCon, "{-c2/-z1 dec.bin >enc.raz} or {-d enc.raz >dec.bin}");

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
		//if(isatty(fileno(stdin))||isatty(fileno(stdout)))
		//	{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}

		const char *fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *in=fopen(fname,"rb");
		if(!in){fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);return 2;}
		poptFreeContext(optCon);

		//lzmaOpen7z();
		int ret=_decompress(in,stdout);
		//lzmaClose7z();
		fclose(in);
		return ret;
	}else{
		const char *fname=poptGetArg(optCon);
		if(!fname){poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *in=fopen(fname,"rb");
		if(!in){fprintf(stderr,"failed to open %s\n",fname);poptFreeContext(optCon);return 2;}
		poptFreeContext(optCon);

		if(zopfli)fprintf(stderr,"(zopfli numiterations %d)\n",zopfli);
		else{
			fprintf(stderr,"compression level = %d ",level);
			if(!lzmaOpen7z())fprintf(stderr,"(7zip)\n");
			else fprintf(stderr,"(zlib)\n");
		}
		int ret=_compress(in,stdout,level+zopfli*10); //lol
		lzmaClose7z();
		fclose(in);
		return ret;
	}
}
