#ifndef _LZMA_H_
#define _LZMA_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#ifdef FEOS
typedef long fpos_t;
#endif
//from BSD libc
typedef int (*tRead)(void *, char *, int);
typedef int (*tWrite)(void *, const char *, int);
typedef int (*tSeek)(void *, long long, int);
typedef long long (*tTell)(void *);
typedef int (*tClose)(void *);

#include <stdbool.h>
int lzmaOpen7z();
bool lzma7zAlive();
int lzmaClose7z();

/*
Archive API
Please see guid.txt
Still todo.
*/
int lzmaCreateArchiver(void **archiver,unsigned char arctype,int encode,int level);
int lzmaDestroyArchiver(void **archiver,int encode);
int lzmaMakeReader(void **_reader,void *h,tRead pRead,tClose pClose,tSeek pSeek,tTell pTell);
int lzmaDestroyReader(void **reader);
//int lzmaOpenArchive(void *archiver,void *reader);
//int lzmaArchiveFileNum(void *archiver,unsigned int *numItems);

/*
Coder API
7zip/Compress/__CODER__Register.cpp

0x040108 Deflate
0x040109 Deflate64
0x040202 BZip2
0x040301 Rar1
0x040302 Rar2
0x040303 Rar3
0x040305 Rar5
0x030401 PPMD
0x030101 LZMA
0x21     LZMA2
0x00     Copy
*/
int lzmaCreateCoder(void **coder,unsigned long long int id,int encode,int level);
int lzmaDestroyCoder(void **coder);
int lzmaCodeOneshot(void *coder, unsigned char *in, size_t isize, unsigned char *out, size_t *osize);
int lzmaCodeCallback(void *coder, void *hin, tRead pRead_in, tClose pClose_in, void *hout, tWrite pWrite_out, tClose pClose_out);

#ifdef __cplusplus
}
#endif

#endif
