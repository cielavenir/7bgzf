#include "../compat.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "xutil.h"

unsigned long long int read64(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24)|( (unsigned long long int)(x[4]|(x[5]<<8)|(x[6]<<16)|((unsigned int)x[7]<<24)) <<32);
}

//#if 0
//write32 is now defined in lzmasdk.c.
//now static there.
unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24);
}
//#endif

unsigned int read24(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16);
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

//#if 0
//write32 is now defined in lzmasdk.c.
//now static there.
void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}
//#endif

void write24(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff;
}

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

unsigned int read24be(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[2]|(x[1]<<8)|(x[0]<<16);
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

void write24be(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[2]=n&0xff,x[1]=(n>>8)&0xff,x[0]=(n>>16)&0xff;
}

void write16be(void *p, const unsigned short n){
	unsigned char *x=(unsigned char*)p;
	x[1]=n&0xff,x[0]=(n>>8)&0xff;
}

char* myfgets(char *buf,int n,FILE *fp){ //accepts LF/CRLF
	char *ret=fgets(buf,n,fp);
	if(!ret)return NULL;
	if(strlen(buf)&&buf[strlen(buf)-1]=='\n')buf[strlen(buf)-1]=0;
	if(strlen(buf)&&buf[strlen(buf)-1]=='\r')buf[strlen(buf)-1]=0;
	return ret;
}

void msleep(int msec){
#if defined(_WIN32)
	Sleep(msec);
#elif !defined(FEOS)
	if(msec>999)sleep(msec/1000);
	if(msec%1000)usleep(msec%1000*1000);
#endif
}

int strchrindex(const char *s, const int c, const int idx){
	const char *ret=strchr(s+idx,c);
	if(!ret)return -1;
	return ret-s;
}

unsigned int txt2bin(const char *src, unsigned char *dst, unsigned int len){
	unsigned int i=0;
	for(;i<len;i++){
		unsigned char src0=src[2*i];
		if(!src0||src0==' '||src0=='\t'||src0=='\r'||src0=='\n'||src0=='#'||src0==';'||src0=='\''||src0=='"')break;
		if('a'<=src0&&src0<='z')src0-=0x20;
		if(!( isdigit(src0)||('A'<=src0&&src0<='F') )){fprintf(stderr,"Invalid character %c\n",src0);exit(-1);}
		src0=isdigit(src0)?(src0-'0'):(src0-55);

		unsigned char src1=src[2*i+1];
		if('a'<=src1&&src1<='z')src1-=0x20;
		if(!( isdigit(src1)||('A'<=src1&&src1<='F') )){fprintf(stderr,"Invalid character %c\n",src1);exit(-1);}
		src1=isdigit(src1)?(src1-'0'):(src1-55);
		dst[i]=(src0<<4)|src1;
	}
	return i;
}

size_t _FAT_directory_mbstoucs2(unsigned short* dst, const unsigned char* src, size_t len){
	size_t i=0,j=0;
	for(;src[i];){
		if((src[i]&0x80) == 0x00){
			if(!dst)j++;else{
				if(len-j<2)break;
				dst[j++] = ((src[i  ] & 0x7f)     );
			}
			i++;
		}else if ((src[i] & 0xe0) == 0xc0 ){
			if(!dst)j++;else{
				if(len-j<2)break;
				dst[j++] = ((src[i  ] & 0x3f) << 6)
					  | ((src[i+1] & 0x3f)     );
			}
			i+=2;
		}else if ((src[i] & 0xf0) == 0xe0 ){
			if(!dst)j++;else{
				if(len-j<2)break;
				dst[j++] = ((src[i  ] & 0x0f) << 12)
					  | ((src[i+1] & 0x3f) <<  6)
					  | ((src[i+2] & 0x3f)      );
			}
			i+=3;
		}else if ((src[i] & 0xf8) == 0xf0 ){
			if(!dst)j+=2;else{
				unsigned short z = ((src[i  ] & (unsigned short)0x03) <<  8)  // 2
						   | ((src[i+1] & 0x3f) <<  2)  // 6
						   | ((src[i+2] & 0x30) >>  4); // 2
				if(len-j<2)break;
				dst[j++] = (z-0x40) | 0xd800;
				dst[j++] = ((src[i+2] & 0x0f) <<  6)           //4
					  | ((src[i+3] & 0x3f)      ) | 0xdc00; //6
			}
			i+=4;
		}else break; //cannot convert
	}
	if(dst)dst[j]=0;
	return j;
}

size_t mbstoucs2(unsigned short* dst, const unsigned char* src){
	return _FAT_directory_mbstoucs2(dst,src,256);
}

size_t _FAT_directory_ucs2tombs(unsigned char* dst, const unsigned short* src, size_t len){
	size_t i=0,j=0;
	for (;src[i];i++){
		if(src[i] <= 0x007f){
			if(!dst)j++;else{
				if(len-j<2)break;
				dst[j++] = ((src[i] & 0x007f)      );
			}
		}else if(src[i] <= 0x07ff){
			if(!dst)j+=2;else{
				if(len-j<3)break;
				dst[j++] = ((src[i] & 0x07c0) >>  6) | 0xc0;
				dst[j++] = ((src[i] & 0x003f)      ) | 0x80;
			}
		}else if((src[i] & 0xdc00) == 0xd800 && (src[i+1] & 0xdc00) == 0xdc00){
			if(!dst)j+=4;else{
				unsigned short z = (src[i]&0x3ff)+0x40;
				if(len-j<5)break;
				dst[j++] = ((z      & 0x0300) >>  8) | 0xf0;   //2
				dst[j++] = ((z      & 0x00fc) >>  2) | 0x80;   //6
				dst[j++] = ((z      & 0x0003) <<  4)           //2
					  | ((src[i+1] & 0x03c0) >>  6) | 0x80; //4
				dst[j++] = ((src[i+1] & 0x003f)      ) | 0x80; //6
			}i++;
		}else{
			if(!dst)j+=3;else{
				if(len-j<4)break;
				dst[j++] = ((src[i] & 0xf000) >> 12) | 0xe0;
				dst[j++] = ((src[i] & 0x0fc0) >>  6) | 0x80;
				dst[j++] = ((src[i] & 0x003f)      ) | 0x80;
			}
		}
	}
	if(dst)dst[j]=0;
	return j;
}

size_t ucs2tombs(unsigned char* dst, const unsigned short* src){
	return _FAT_directory_ucs2tombs(dst,src,768);
}

void NullMemory(void* buf, unsigned int n){
	while(n)((char*)buf)[--n]=0;
}

int memcmp_fast(const void *x,const void *y,unsigned int n){
	int v=0;
	unsigned char *X=(unsigned char*)x;
	unsigned char *Y=(unsigned char*)y;
	for(;n--&&!v;)v=*X++-*Y++;
	return v;
}

void *_memstr(const void *s1, const char *s2, size_t siz1){
	return _memmem(s1,siz1,s2,strlen(s2));
}

int makedir(const char *_dir){
	if(!_dir||!*_dir)return -1;
	char *dir=(char*)_dir; //unsafe, but OK
	int l=strlen(dir),i=0,ret=0;
	for(;i<l;i++){
		int c=dir[i];
		if(c=='\\'||c=='/'){
			dir[i]=0;
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
			ret=mkdir(dir);
#else
			ret=mkdir(dir,0755);
#endif
			dir[i]=c;
		}
	}
	return ret;
}

bool wildmatch(const char *pattern, const char *compare){
	switch(*pattern){
		case '?': //0x3f
			return *compare&&wildmatch(pattern+1,compare+1);
		case '*': //0x2a
			return wildmatch(pattern+1,compare)||(*compare&&wildmatch(pattern,compare+1));
		default:
			if(!*pattern&&!*compare)return true;
			if(*pattern!=*compare)return false;
			return wildmatch(pattern+1,compare+1);
	}
}

bool matchwildcard(const char *wildcard, const char *string){
	return matchwildcard2(wildcard, string, wildmode_string);
}

bool matchwildcard2(const char *wildcard, const char *string, const int iMode){
	int u=0,u1=0,u2=0;
	char pattern[768];
	char compare[768];
	if(!wildcard||!string)return 0;
	strcpy(pattern,wildcard);
	strcpy(compare,string);

	for(u=0;u<strlen(pattern);u++){
		pattern[u]=upcase(pattern[u]);
		if(pattern[u]=='\\')pattern[u]='/';
		if(pattern[u]=='/')u1++;
	}

	for(u=0;u<strlen(compare);u++){
		compare[u]=upcase(compare[u]);
		if(compare[u]=='\\')compare[u]='/';
		if(compare[u]=='/')u2++;
	}

	if(
		(iMode==wildmode_string)||
		(iMode==wildmode_samedir&&u1==u2)||
		(iMode==wildmode_recursive&&u1<=u2)
	)return wildmatch(pattern, compare);
	return false;
}

bool fixpath(const char *src, char *dest){
	if(!src||!dest)return false;
	int i=0,j=0,l=strlen(src);
	strcpy(dest,src);

	for(;i<l;i++){
		if(dest[i]=='\\'||dest[i]=='/'){
			if(i-j==1&&dest[j]=='.')dest[j]='_';
			else if(dest[j]=='.'&&dest[j+1]=='.')dest[j]=dest[j+1]=='_';
			dest[i]='/'; //In unix \\ is not supported!
			j=++i;
		}
	}
	return true;
}

const char* mybasename(const char *src){
/*
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	char sep='\\';
#else
	char sep='/';
#endif
*/
	int i=strlen(src)-1;
	for(;i>=0;i--)if(src[i]=='/'||src[i]=='\\')break;
	return src+i+1;
}
