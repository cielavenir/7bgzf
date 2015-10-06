#ifndef _MEMSTREAM_H_
#define _MEMSTREAM_H_

#ifdef __cplusplus
extern "C"{
#endif
typedef struct{
	unsigned char *p;
	unsigned int  current;
	unsigned int  size;
} memstream;

memstream *mopen(void *p, const unsigned int size, memstream *s);
int mclose(memstream *s);
int mgetc(memstream *s);
int mputc(const int c, memstream *s);
int mrewind(memstream *s);
int mavail(memstream *s);
int mtell(memstream *s);
int mlength(memstream *s);
int mread(void *buf, const unsigned int size, memstream *s);
int mwrite(const void *buf, const unsigned int size, memstream *s);
int mcopy(memstream *to, const unsigned int size, memstream *s);
int mseek(memstream *s, const int offset, const int whence);
unsigned int mread32(memstream *s);
unsigned short mread16(memstream *s);
unsigned char mread8(memstream *s);
int mwrite32(const unsigned int n, memstream *s);
int mwrite16(const unsigned short n, memstream *s);
int mwrite8(const unsigned char n, memstream *s);
#ifdef __cplusplus
}
#endif

#endif
