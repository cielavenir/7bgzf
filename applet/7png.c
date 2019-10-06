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

unsigned int read32be(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[3]|(x[2]<<8)|(x[1]<<16)|((unsigned int)x[0]<<24);
}
void write32be(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[3]=n&0xff,x[2]=(n>>8)&0xff,x[1]=(n>>16)&0xff,x[0]=(n>>24)&0xff;
}

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

#else
#include "../cielbox.h"
#endif

#include "../lib/lzma.h"
#include "../lib/popt/popt.h"

#include <sys/time.h>

#define MAX_IDAT 1024
static unsigned char* tbl[MAX_IDAT];
static unsigned int tbl_size[MAX_IDAT];

static int _compress(FILE *in, FILE *out, int level, int method, bool strip){
	void* coder=NULL;
	lzmaCreateCoder(&coder,0x040108,1,level);

	fread(buf,1,8,in);
	if(memcmp(buf,"\x89PNG\x0d\x0a\x1a\x0a",8)){
		fprintf(stderr,"not PNG file\n");
		return -1;
	}
	fwrite(buf,1,8,out);

	//iterate png chunks
	bool cgbi=false;
	unsigned int cgbiflg=0;
	int bits=0,color=0,filter=0,width=0,height=0;

	int tbl_last=0;
	size_t compsize=0;
	for(;;){
		if(fread(buf,1,8,in)<8)break;
		unsigned int len=read32be(buf);
		unsigned int type=read32(buf+4);
		if((level||cgbi) && type==fourcc('I','D','A','T')){
			//recompress
			if(tbl_last==MAX_IDAT){
				fprintf(stderr,"Sorry, the number of IDAT chunks is limited to %d.\n",MAX_IDAT);
				return -1;
			}
			fprintf(stderr,"IDAT data length=%d\n",len);
			tbl_size[tbl_last]=len;
			tbl[tbl_last]=(unsigned char*)malloc(len);
			fread(tbl[tbl_last],1,len,in);
			compsize+=len;
			tbl_last++;
			fread(buf,1,4,in);
		}else if((level||cgbi) && type==fourcc('I','E','N','D')){
			fprintf(stderr,"compressed length=%d\n",(int)compsize);
			size_t bufsize=compsize + (compsize>>1);

			int declen=0;
			{
				z_stream z;

				z.zalloc = Z_NULL;
				z.zfree = Z_NULL;
				z.opaque = Z_NULL;

				{
					int x=cgbi?inflateInit2(&z,-MAX_WBITS):inflateInit(&z);
					if(x != Z_OK){
						fprintf(stderr,"inflateInit: %s\n", (z.msg) ? z.msg : "???");
						return 1;
					}
				}

				int i=0;
				for(;i<tbl_last;i++){
					z.next_in = tbl[i];
					z.avail_in = tbl_size[i];
					int status = Z_OK;
					for(;status!=Z_STREAM_END && (i==tbl_last-1||z.avail_in);){
						z.next_out = buf;
						z.avail_out = BUFLEN;

						status = inflate(&z, Z_FINISH);
						if(status != Z_STREAM_END && status != Z_OK && status != Z_BUF_ERROR){
							fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
							return 10;
						}
						declen+=BUFLEN-z.avail_out;
					}
				}
				if(inflateEnd(&z) != Z_OK){
					fprintf(stderr,"inflateEnd: %s\n", (z.msg) ? z.msg : "???");
					return 2;
				}
			}

			fprintf(stderr,"decode length=%d\n",declen);
			unsigned char* __decompbuf=(unsigned char*)malloc(declen);
			if(!__decompbuf){
				fprintf(stderr,"Failed to allocate decompressed buffer\n");
				return -1;
			}

			{
				z_stream z;

				z.zalloc = Z_NULL;
				z.zfree = Z_NULL;
				z.opaque = Z_NULL;

				{
					int x=cgbi?inflateInit2(&z,-MAX_WBITS):inflateInit(&z);
					if(x != Z_OK){
						fprintf(stderr,"inflateInit: %s\n", (z.msg) ? z.msg : "???");
						return 1;
					}
				}

				int i=0;
				z.next_out = __decompbuf;
				z.avail_out = declen;
				for(;i<tbl_last;i++){
					z.next_in = tbl[i];
					z.avail_in = tbl_size[i];

					int status = inflate(&z, Z_FINISH);
					if(status != Z_STREAM_END && status != Z_OK && status != Z_BUF_ERROR){
						fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
						return 10;
					}
					free(tbl[i]);
				}
				if(inflateEnd(&z) != Z_OK){
					fprintf(stderr,"inflateEnd: %s\n", (z.msg) ? z.msg : "???");
					return 2;
				}
			}

			if(cgbi&&cgbiflg){
				int i=1,j=0;
				if(color==6){ //RGBA
					if(bits==8){
						for(;i<declen;i+=4){
							__decompbuf[i]^=__decompbuf[i+2];
							__decompbuf[i+2]^=__decompbuf[i];
							__decompbuf[i]^=__decompbuf[i+2];
							if(++j==width)j=0,++i;
						}
					}
					if(bits==16){
						for(;i<declen;i+=8){
							__decompbuf[i+0]^=__decompbuf[i+4];
							__decompbuf[i+4]^=__decompbuf[i+0];
							__decompbuf[i+0]^=__decompbuf[i+4];
							__decompbuf[i+1]^=__decompbuf[i+5];
							__decompbuf[i+5]^=__decompbuf[i+1];
							__decompbuf[i+1]^=__decompbuf[i+5];
							if(++j==width)j=0,++i;
						}
					}
				}
				if(color==2){ //RGB
					if(bits==8){
						for(;i<declen;i+=3){
							__decompbuf[i]^=__decompbuf[i+2];
							__decompbuf[i+2]^=__decompbuf[i];
							__decompbuf[i]^=__decompbuf[i+2];
							if(++j==width)j=0,++i;
						}
					}
					if(bits==16){
						for(;i<declen;i+=6){
							__decompbuf[i+0]^=__decompbuf[i+4];
							__decompbuf[i+4]^=__decompbuf[i+0];
							__decompbuf[i+0]^=__decompbuf[i+4];
							__decompbuf[i+1]^=__decompbuf[i+5];
							__decompbuf[i+5]^=__decompbuf[i+1];
							__decompbuf[i+1]^=__decompbuf[i+5];
							if(++j==width)j=0,++i;
						}
					}
				}
			}

			unsigned char* __compbuf=(unsigned char*)malloc(bufsize);
			if(!__compbuf){
				fprintf(stderr,"Failed to allocate compressed buffer\n");
				return -1;
			}

			zlibutil_buffer zlibbuf;
			memset(&zlibbuf,0,sizeof(zlibutil_buffer));
			zlibbuf.dest=__compbuf;
			zlibbuf.destLen=bufsize;
			zlibbuf.source=__decompbuf;
			zlibbuf.sourceLen=declen;
			zlibbuf.encode=1;
			zlibbuf.level=level;
			zlibbuf.rfc1950=1;

			if(method==DEFLATE_ZLIB){
				zlibbuf.func = zlib_deflate;
			}else if(method==DEFLATE_7ZIP){
				zlibbuf.func = lzma_deflate;
			}else if(method==DEFLATE_ZOPFLI){
				zlibbuf.func = zopfli_deflate;
			}else if(method==DEFLATE_MINIZ){
				zlibbuf.func = miniz_deflate;
			}else if(method==DEFLATE_SLZ){
				slz_initialize();
				zlibbuf.func = slz_deflate;
			}else if(method==DEFLATE_LIBDEFLATE){
				zlibbuf.func = libdeflate_deflate;
			}else if(method==DEFLATE_ZLIBNG){
				zlibbuf.func = zlibng_deflate;
			}else if(method==DEFLATE_IGZIP){
				zlibbuf.func = igzip_deflate;
			}

			zlibutil_buffer_code(&zlibbuf);

			if(zlibbuf.ret){
				if(method==DEFLATE_ZLIB){
					fprintf(stderr,"deflate %d\n",zlibbuf.ret);
				}else if(method==DEFLATE_7ZIP){
					fprintf(stderr,"NDeflate::CCoder::Code %d\n",zlibbuf.ret);
				}else if(method==DEFLATE_ZOPFLI){
					fprintf(stderr,"zopfli_deflate %d\n",zlibbuf.ret);
				}else if(method==DEFLATE_MINIZ){
					fprintf(stderr,"miniz_deflate %d\n",zlibbuf.ret);
				}else if(method==DEFLATE_SLZ){
					fprintf(stderr,"slz_deflate %d\n",zlibbuf.ret);
				}else if(method==DEFLATE_LIBDEFLATE){
					fprintf(stderr,"libdeflate_deflate %d\n",zlibbuf.ret);
				}else if(method==DEFLATE_ZLIBNG){
					fprintf(stderr,"zng_deflate %d\n",zlibbuf.ret);
				}else if(method==DEFLATE_IGZIP){
					fprintf(stderr,"isal_deflate %d\n",zlibbuf.ret);
				}
				free(__compbuf);
				return 1;
			}

			write32be(buf,zlibbuf.destLen);
			unsigned int crc=0;
			write32(buf+4,fourcc('I','D','A','T'));
			crc=crc32(crc,buf+4,4);
			fwrite(buf,1,8,out);

			crc=crc32(crc,zlibbuf.dest,zlibbuf.destLen);
			fwrite(zlibbuf.dest,1,zlibbuf.destLen,out);

			write32be(buf,crc);
			fwrite(buf,1,4,out);
			fprintf(stderr,"recompressed length=%d\n",(int)zlibbuf.destLen);
			free(__compbuf);

			write32be(buf,0);
			write32(buf+4,fourcc('I','E','N','D'));
			write32be(buf+8,0xae426082);
			fwrite(buf,1,12,out);
		}else{
			bool copy=!strip || type==fourcc('I','H','D','R') || type==fourcc('P','L','T','E') || type==fourcc('t','R','N','S') || type==fourcc('I','D','A','T') || type==fourcc('I','E','N','D');
			if(type==fourcc('C','g','B','I')){
				cgbi=true;
				copy=false;
			}
			//copy
			len+=4; //footer crc
			if(copy)fwrite(buf,1,8,out);
			for(;len;){
				fread(buf,1,min(BUFLEN,len),in);
				if(copy)fwrite(buf,1,min(BUFLEN,len),out);
				if(type==fourcc('C','g','B','I')){
					cgbiflg=read32be(buf); //use head 4 bytes
					if(level==0 && cgbiflg){
						fprintf(stderr,"CgBI RGBA<=>BGRA convertion is required; force recompression.\n");
						level=2; //fixme
					}
				}
				if(type==fourcc('I','H','D','R')){
					width=read32be(buf);
					height=read32be(buf+4);
					bits=buf[8];
					color=buf[9];
					filter=buf[11];
					if(cgbi && (buf[12]/*interlace*/ || (bits!=8&&bits!=16) || (color!=2&&color!=6))){
						fprintf(stderr,"Sorry: CgBI RGBA<=>BGRA convertion is omitted because the routine is incomplete.\n");
						cgbiflg=0;
					}
				}
				len-=min(BUFLEN,len);
			}
		}
	}
	fprintf(stderr,"Done.\n");
	lzmaDestroyCoder(&coder);
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int _7png(const int argc, const char **argv){
#endif
	int cmode=0,mode=0;
	int zlib=0,sevenzip=0,zopfli=0,miniz=0,slz=0,libdeflate=0,zlibng=0,igzip=0;
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
		{ "igzip",     'i',         POPT_ARG_INT|POPT_ARGFLAG_OPTIONAL, NULL,    'i',       "1-4 (default 1) igzip (1 becomes igzip internal level 0, 2 becomes 1, ...)", "level" },
		{ "zopfli",     'Z',         POPT_ARG_INT, &zopfli,    0,       "zopfli", "numiterations" },
		//{ "threshold",  't',         POPT_ARG_INT, &threshold, 0,       "compression threshold (in %, 10-100)", "threshold" },
		{ "strip", 't',         POPT_ARG_NONE,            &mode,      0,       "strip", "strip unnecessary chunks" },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "-S9 [-t] < before.png > after.png (only recompression is available)");

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
		(!mode&&!zlib&&!sevenzip&&!zopfli&&!miniz&&!slz&&!libdeflate&&!zlibng&&!igzip)// ||
		//(mode&&(zlib||sevenzip||zopfli||miniz||slz||libdeflate||zlibng||igzip)) ||
		//(!mode&&(level_sum==zlib)+(level_sum==sevenzip)+(level_sum==zopfli)+(level_sum==miniz)+(level_sum==slz)+(level_sum==libdeflate)+(level_sum==zlibng)+(level_sum==igzip)!=1)
	){
		poptPrintHelp(optCon, stderr, 0);
		poptFreeContext(optCon);

		fprintf(stderr,"\nUsing \"-t\" will cause only to strip unnecessary chunks without recompression.\n");

		if(!lzmaOpen7z())fprintf(stderr,"Note: 7-zip is AVAILABLE.\n"),lzmaClose7z();
		else fprintf(stderr,"Note: 7-zip is NOT available.\n");
		return 1;
	}

	if(isatty(fileno(stdin))||isatty(fileno(stdout)))
		{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
	poptFreeContext(optCon);

	fprintf(stderr,"compression level = %d ",level_sum);
	struct timeval tstart,tend;
	gettimeofday(&tstart,NULL);
	int ret=0;
	if(zlib){
		fprintf(stderr,"(zlib)\n");
		ret=_compress(stdin,stdout,zlib,DEFLATE_ZLIB,mode);
	}else if(sevenzip){
		fprintf(stderr,"(7zip)\n");
		if(lzmaOpen7z()){
			fprintf(stderr,"7-zip is NOT available.\n");
			return -1;
		}
		ret=_compress(stdin,stdout,sevenzip,DEFLATE_7ZIP,mode);
		lzmaClose7z();
	}else if(zopfli){
		fprintf(stderr,"(zopfli)\n");
		ret=_compress(stdin,stdout,zopfli,DEFLATE_ZOPFLI,mode);
	}else if(miniz){
		fprintf(stderr,"(miniz)\n");
		ret=_compress(stdin,stdout,miniz,DEFLATE_MINIZ,mode);
	}else if(slz){
		fprintf(stderr,"(slz)\n");
		ret=_compress(stdin,stdout,slz,DEFLATE_SLZ,mode);
	}else if(libdeflate){
		fprintf(stderr,"(libdeflate)\n");
		ret=_compress(stdin,stdout,libdeflate,DEFLATE_LIBDEFLATE,mode);
	}else if(zlibng){
		fprintf(stderr,"(zlibng)\n");
		ret=_compress(stdin,stdout,zlibng,DEFLATE_ZLIBNG,mode);
	}else if(igzip){
		fprintf(stderr,"(igzip)\n");
		ret=_compress(stdin,stdout,igzip,DEFLATE_IGZIP,mode);
	}else if(mode){
		fprintf(stderr,"(strip)\n");
		ret=_compress(stdin,stdout,0,0,mode);
	}
	gettimeofday(&tend,NULL);
	fprintf(stderr,"ellapsed time: %.6f sec\n",(tend.tv_sec+tend.tv_usec*0.000001)-(tstart.tv_sec+tstart.tv_usec*0.000001));
	return ret;
}
