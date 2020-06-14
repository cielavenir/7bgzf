#ifndef __CIELBOX_H__
#define __CIELBOX_H__

#define BOX_REVISION 200614

#ifdef __cplusplus
extern "C"{
#endif
#include "compat.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>

#ifndef FEOS
#include <utime.h>
#endif

#ifndef NO_ZLIB
#include "lib/zlibutil.h"
#endif

//64KB
#define DECOMPBUFLEN (1<<16)
#define COMPBUFLEN   (DECOMPBUFLEN|(DECOMPBUFLEN>>1))
extern unsigned char __compbuf[COMPBUFLEN],__decompbuf[DECOMPBUFLEN];

#include "lib/xutil.h"
#include "lib/xorshift.h"

unsigned short crc16(unsigned short crc, const unsigned char *p, unsigned int
size);
unsigned int crc32_left(unsigned int crc, const unsigned char *p, unsigned int
 size);
typedef void (*type_u32p)(u32*);

//applet
#define F(name) int name(const int argc, const char **argv);
F(applets)
F(_install)
F(_7bgzf)
F(_7ciso)
F(_7daxcr)
F(_7dictzip)
F(_7gzinga)
F(_7gzip)
F(_7migz)
F(_7png)
F(_7razf)
F(zlibrawstdio)
F(zlibrawstdio2)
#undef F

typedef struct{
	const char *name;
	int  (*func)(const int, const char**);
}applet;

#ifdef __cplusplus
}
#endif

#endif
