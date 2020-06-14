#ifndef _XUTIL_H_
#define _XUTIL_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include <stdbool.h>

unsigned long long read64(const void *p);
unsigned int read32(const void *p);
unsigned int read24(const void *p);
unsigned short read16(const void *p);

void write64(void *p, const unsigned long long n);
void write32(void *p, const unsigned int n);
void write24(void *p, const unsigned int n);
void write16(void *p, const unsigned short n);

unsigned long long read64be(const void *p);
unsigned int read32be(const void *p);
unsigned int read24be(const void *p);
unsigned short read16be(const void *p);

void write64be(void *p, const unsigned long long n);
void write32be(void *p, const unsigned int n);
void write24be(void *p, const unsigned int n);
void write16be(void *p, const unsigned short n);

//accepts LF/CRLF
char* myfgets(char *buf,int n,FILE *fp);

void msleep(int msec);
int strchrindex(const char *s, const int c, const int idx);
unsigned int txt2bin(const char *src, unsigned char *dst, unsigned int len);

size_t _FAT_directory_mbstoucs2(unsigned short* dst, const unsigned char* src, size_t len);
size_t mbstoucs2(unsigned short* dst, const unsigned char* src);
size_t _FAT_directory_ucs2tombs(unsigned char* dst, const unsigned short* src, size_t len);
size_t ucs2tombs(unsigned char* dst, const unsigned short* src);

void NullMemory(void* buf, unsigned int n);
int memcmp_fast(const void *x,const void *y,unsigned int n);

void *_memmem(const void *s1, size_t siz1, const void *s2, size_t siz2);
void *_memstr(const void *s1, const char *s2, size_t siz1);

int makedir(const char *_dir);
bool wildmatch(const char *pattern, const char *compare);

enum{
	wildmode_string,
	wildmode_samedir,
	wildmode_recursive,
};

bool matchwildcard(const char *wildcard, const char *string);
bool matchwildcard2(const char *wildcard, const char *string, const int iMode);

bool fixpath(const char *src, char *dest);
const char* mybasename(const char *src);

#ifdef __cplusplus
}
#endif

#endif
