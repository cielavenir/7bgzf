// BGZF_METHOD=slz1 LD_PRELOAD=./7bgzf.so [samtools|bcftools]
// samtools/bcftools need to link libhts.so (not a)

#include "compat.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#define alloca _alloca
#else
#include <alloca.h>
#endif

#include "lib/zlibutil.h"
#include "lib/lzma.h"

static void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}
static void write16(void *p, const unsigned short n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff;
}

static bool loaded7z=false;
//__attribute__((constructor)) void init(){
//      lzmaOpen7z();
//}
__attribute__((destructor)) void finish(){
        if(loaded7z)lzmaClose7z();
}

static int method=-1;
static int level=-1;

int bgzf_compress(void *_dst, size_t *_dlen, const void *src, size_t slen, int level_unused){
	if(!slen){
		if(*_dlen<28)return -1;
		*_dlen=28;
		memcpy(_dst,
			"\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\xff" // header         (10)
			"\x06\0BC\x02\x00"                         // extra header   (6)
			"\x1b\x00"                                 // size(28-1)     (2)
			"\x03\x00"                                 // null deflation (2)
			"\x00\x00\x00\x00\x00\x00\x00\x00"         // footer         (8)
			,28);
		return 0;
	}

	if(method==-1){
		method=DEFLATE_ZLIB;
		const char* _smethod=getenv("BGZF_METHOD");
		if(_smethod&&*_smethod){
			char *smethod=alloca(strlen(_smethod)+1);
			strcpy(smethod,_smethod);
			//fputs(smethod,stderr);fputs("\n",stderr);
			//set level
			int _level=-1;
			int l=strlen(smethod);
			int i=l-1;
			for(;i>=0 && '0'<=smethod[i]&&smethod[i]<='9';i--){
				if(_level<0)_level=0;
				_level*=10;
				_level+=smethod[i]-'0';
			}
			if(_level>=0)level=_level;
			//set method
			smethod[i+1]=0;
			if(!strcasecmp(smethod,"zlib")){
				method=DEFLATE_ZLIB;
			}
			if(!strcasecmp(smethod,"7zip") || !strcasecmp(smethod,"7-zip")){
				method=DEFLATE_7ZIP;
			}
			if(!strcasecmp(smethod,"zopfli")){
				method=DEFLATE_ZOPFLI;
			}
			if(!strcasecmp(smethod,"miniz")){
				method=DEFLATE_MINIZ;
			}
			if(!strcasecmp(smethod,"slz") || !strcasecmp(smethod,"libslz")){
				method=DEFLATE_SLZ;
			}
			if(!strcasecmp(smethod,"libdeflate")){
				method=DEFLATE_LIBDEFLATE;
			}
			if(!strcasecmp(smethod,"zlibng")){
				method=DEFLATE_ZLIBNG;
			}
			if(!strcasecmp(smethod,"igzip")){
				method=DEFLATE_IGZIP;
			}
			if(!strcasecmp(smethod,"cryptopp")){
				method=DEFLATE_CRYPTOPP;
			}
		}

		if(level<0){
			if(method==DEFLATE_ZLIB)level=6;
			if(method==DEFLATE_7ZIP)level=2;
			if(method==DEFLATE_ZOPFLI)level=1;
			if(method==DEFLATE_MINIZ)level=1;
			if(method==DEFLATE_SLZ)level=1;
			if(method==DEFLATE_LIBDEFLATE)level=6;
			if(method==DEFLATE_ZLIBNG)level=6;
			if(method==DEFLATE_IGZIP)level=1;
			if(method==DEFLATE_CRYPTOPP)level=6;
		}
	}

	//compress
	if(*_dlen<26)return -1;
	size_t dlen=*_dlen-26;
	unsigned char *dst=((unsigned char*)_dst)+18;

	if(method==DEFLATE_ZLIB){
		int r=zlib_deflate(dst,&dlen,(const unsigned char*)src,slen,level);
		if(r){
			fprintf(stderr,"deflate %d\n",r);
			return 1;
		}
	}
	if(method==DEFLATE_7ZIP){
		if(!loaded7z)lzmaOpen7z();
		void *coder=NULL;
		lzmaCreateCoder(&coder,0x040108,1,level);
		if(coder){
			int r=lzmaCodeOneshot(coder,(unsigned char*)src,slen,dst,&dlen);
			lzmaDestroyCoder(&coder);
			if(r){
				fprintf(stderr,"NDeflate::CCoder::Code %d\n",r);
				return 1;
			}
		}else{
			return -1;
		}
	}
	if(method==DEFLATE_ZOPFLI){
		int r=zopfli_deflate(dst,&dlen,src,slen,level);
		if(r){
			fprintf(stderr,"zopfli_deflate %d\n",r);
			return 1;
		}
	}
	if(method==DEFLATE_MINIZ){
		int r=miniz_deflate(dst,&dlen,src,slen,level);
		if(r){
			fprintf(stderr,"miniz_deflate %d\n",r);
			return 1;
		}
	}
	if(method==DEFLATE_SLZ){
		int r=slz_deflate(dst,&dlen,src,slen,level);
		if(r){
			fprintf(stderr,"slz_deflate %d\n",r);
			return 1;
		}
	}
	if(method==DEFLATE_LIBDEFLATE){
		int r=libdeflate_deflate(dst,&dlen,src,slen,level);
		if(r){
			fprintf(stderr,"libdeflate_deflate %d\n",r);
			return 1;
		}
	}
	if(method==DEFLATE_ZLIBNG){
		int r=zlibng_deflate(dst,&dlen,(const unsigned char*)src,slen,level);
		if(r){
			fprintf(stderr,"zng_deflate %d\n",r);
			return 1;
		}
	}
	if(method==DEFLATE_IGZIP){
		int r=igzip_deflate(dst,&dlen,(const unsigned char*)src,slen,level);
		if(r){
			fprintf(stderr,"isal_deflate %d\n",r);
			return 1;
		}
	}
	if(method==DEFLATE_CRYPTOPP){
		int r=cryptopp_deflate(dst,&dlen,(const unsigned char*)src,slen,level);
		if(r){
			fprintf(stderr,"cryptopp_deflate %d\n",r);
			return 1;
		}
	}
	*_dlen=dlen+25;
	memcpy(_dst,"\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\xff\x06\0BC\x02\x00",16);
	write16(_dst+16,*_dlen);
	write32(dst+dlen,crc32(0,src,slen));
	write32(dst+dlen+4,slen);
	*_dlen+=1;
	return 0;
}
