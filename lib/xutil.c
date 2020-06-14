#include "../compat.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "xutil.h"

unsigned long long int read64(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24)|( (unsigned long long int)(x[4]|(x[5]<<8)|(x[6]<<16)|((unsigned int)x[7]<<24)) <<32);
}

#if 0
//write32 is now defined in lzmasdk.c.
unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24);
}
#endif

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

#if 0
//write32 is now defined in lzmasdk.c.
void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}
#endif

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
