#ifndef __COMPAT_H__
#define __COMPAT_H__

#ifdef __cplusplus
extern "C"{
#endif

#define _FILE_OFFSET_BITS 64

typedef unsigned char byte;
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

typedef char  s8;
typedef short s16;
typedef int   s32;
typedef long long s64;

#if !defined(__cplusplus)
//typedef enum { false, true } bool;
#include <stdbool.h>
#endif

#if !defined(__cplusplus) && !defined(min)
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define fourcc(a,b,c,d) ((u32)(a) | ((u32)(b)<<8) | ((u32)(c)<<16) | ((u32)(d)<<24))

#define between(a,x,b) ((a)<=(x)&&(x)<=(b))
#define upcase(c) (between('a',(unsigned char)(c),'z')?c-' ':c)
#define downcase(c) (between('A',(unsigned char)(c),'Z')?c+' ':c)

//I hope (sizeof(val)*CHAR_BIT-(rot)) will be precalculated in compilation.
#define lrotr(val,rot) (( (val)<<(sizeof(val)*CHAR_BIT-(rot)) )|( (val)>>(rot) ))
#define lrotl(val,rot) (( (val)<<(rot) )|( (val)>>(sizeof(val)*CHAR_BIT-(rot)) ))

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	#include <windows.h>
	#include <fcntl.h>
	#include <io.h>
	#define sleep(t) Sleep(1000*(t))
	#define initstdio() setmode(fileno(stdin),O_BINARY),setmode(fileno(stdout),O_BINARY),setmode(fileno(stderr),O_BINARY);
	//bah, msvcrt.a is obsolete...?
	#define filelengthi64(f) filelength(f)

	#define OPEN_BINARY O_BINARY
	//because of nasty msvcrt
	#define LLD "I64d"
	#define LLU "I64u"
	#define LLX "I64x"
#else
	#include <unistd.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	int filelength(int fd);
	long long filelengthi64(int fd);
	#define initstdio()
	#define OPEN_BINARY 0
	#define LLD "lld"
	#define LLU "llu"
	#define LLX "llx"
#endif

int sfilelength(const char *path);
long long sfilelengthi64(const char *path);
int filemode(int fd);
int sfilemode(const char *path);

//p should be 2,4,8,...
#define align2p(p,i) (((i)+((p)-1))&~((p)-1))

#define align2(i) align2p(2,i)
#define align4(i) align2p(4,i)
#define align8(i) align2p(8,i)
#define align256(i) align2p(256,i)
#define align512(i) align2p(512,i)
#define swiCRC16 crc16

#define BIT(n) (1<<(n))

#define isRedirected(file) (!isatty(fileno(file)))

//currently 4MB. some game cheat entry is more than 3MB...
#define BUFLEN (1<<22)

extern unsigned char buf[BUFLEN];
#define cbuf ((char*)buf)

#ifdef __cplusplus
}
#endif

#endif
