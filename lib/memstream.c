#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "memstream.h"

memstream *mopen(void *p, const unsigned int size, memstream *s){
	memstream *m=s?s:(memstream*)malloc(sizeof(memstream));
	if(!m)return NULL;
	m->p=(unsigned char*)p;
	m->current=0;
	m->size=size;
	return m;
}

int mclose(memstream *s){ //do not call mclose if you call mopen using existing memstream.
	if(!s)return EOF;
	free(s);
	return 0;
}

int mgetc(memstream *s){
	if(!s||s->current==s->size)return EOF;
	return s->p[s->current++];
}

int mputc(const int c, memstream *s){
	if(!s||s->current==s->size)return EOF;
	s->p[s->current++]=c&0xff;
	return c&0xff;
}

int mrewind(memstream *s){
	if(!s)return EOF;
	s->current=0;
	return 0;
}

int mavail(memstream *s){
	if(!s)return EOF;
	return s->size-s->current;
}

int mtell(memstream *s){
	if(!s)return EOF;
	return s->current;
}

int mlength(memstream *s){
	if(!s)return EOF;
	return s->size;
}

int mread(void *buf, const unsigned int size, memstream *s){
	int i=0;
	unsigned char *p=(unsigned char*)buf;
	if(!s||!p)return EOF;
	for(;i<size&&s->current<s->size;)p[i++]=s->p[s->current++];
	return i;
}

int mwrite(const void *buf, const unsigned int size, memstream *s){
	int i=0;
	unsigned char *p=(unsigned char*)buf;
	if(!s||!p)return EOF;
	for(;i<size&&s->current<s->size;)s->p[s->current++]=p[i++];
	return i;
}

int mcopy(memstream *to, const unsigned int size, memstream *s){
	int _size=size;
	if(_size>mavail(s))_size=mavail(s);
	if(_size>mavail(to))_size=mavail(to);
	if(!_size)return 0;

	mread((unsigned char*)(to->p)+mtell(to),_size,s);mseek(to,_size,SEEK_CUR);
	//mwrite((unsigned char*)(s->p)+mtell(s),_size,to);mseek(s,_size,SEEK_CUR);

	return _size;
}

int mseek(memstream *s, const int offset, const int whence){
	if(!s)return EOF;
	switch(whence){
		case SEEK_SET:
			if(offset<0||s->size<offset)return EOF;
			s->current=offset;
			return s->current;
		case SEEK_CUR:
			if((offset<0&&s->current<-offset)||(offset>0&&s->size<s->current+offset))return EOF;
			s->current+=offset;
			return s->current;
		case SEEK_END:
			if(offset>0||s->size<-offset)return EOF;
			s->current=s->size+offset;
			return s->current;
	}
	return EOF;
}

unsigned int mread32(memstream *s){
	if(!s||s->size-s->current<4)return 0;
	const unsigned char *x=(const unsigned char*)s->p+s->current;
	s->current+=4;
	return x[0]|(x[1]<<8)|(x[2]<<16)|(x[3]<<24);
}

unsigned short mread16(memstream *s){
	if(!s||s->size-s->current<2)return 0;
	const unsigned char *x=(const unsigned char*)s->p+s->current;
	s->current+=2;
	return x[0]|(x[1]<<8);
}

unsigned char mread8(memstream *s){
	if(!s||s->size-s->current<1)return 0;
	const unsigned char *x=(const unsigned char*)s->p+s->current;
	s->current+=1;
	return x[0];
}

int mwrite32(const unsigned int n, memstream *s){
	if(!s||s->size-s->current<4)return EOF;
	unsigned char *x=(unsigned char*)s->p+s->current;
	s->current+=4;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
	return 0;
}

int mwrite16(const unsigned short n, memstream *s){
	if(!s||s->size-s->current<2)return EOF;
	unsigned char *x=(unsigned char*)s->p+s->current;
	s->current+=2;
	x[0]=n&0xff,x[1]=(n>>8)&0xff;
	return 0;
}

int mwrite8(const unsigned char n, memstream *s){
	if(!s||s->size-s->current<1)return EOF;
	unsigned char *x=(unsigned char*)s->p+s->current;
	s->current+=1;
	x[0]=n&0xff;
	return 0;
}
