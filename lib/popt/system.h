/**
 * \file popt/system.h
 */

//#ifdef HAVE_CONFIG_H
//# ifndef _MSC_VER
//#  include "config.h"
//# else
//#  include "config_msvc.h"
//# endif /* _MSC_VER */
//#endif /* HAVE_CONFIG_H */

/*
customize:
- no iconv/gettext dependency
- disabled POPT_fprintf() and poptReadDefaultConfig()
- poptlookup3.c is separated
*/

#if defined(_MSC_VER) || defined(__MINGW32__)
# define _CRT_SECURE_NO_WARNINGS 1
#endif

#define POPT_fprintf fprintf

#define PACKAGE "popt"

#if defined(__ANDROID__) || defined(FEOS) || defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
#define HAVE_STPCPY
#endif

#define HAVE_STRERROR
//#define HAVE_SRANDOM //Win32
//#define HAVE_VASPRINTF

#if defined(FEOS)
#include <stdio.h> //SEEK_CUR
#else
#define HAVE_FLOAT_H
#endif

//access
#include <unistd.h>

#if defined (__GLIBC__) && defined(__LCLINT__)
/*@-declundef@*/
/*@unchecked@*/
extern __const __int32_t *__ctype_tolower;
/*@unchecked@*/
extern __const __int32_t *__ctype_toupper;
/*@=declundef@*/
#endif

#include <ctype.h>

/* XXX isspace(3) has i18n encoding signednesss issues on Solaris. */
#define	_isspaceptr(_chp)	isspace((int)(*(unsigned const char *)(_chp)))

#ifdef HAVE_MCHECK_H
#include <mcheck.h>
#endif

#include <stdlib.h>
#include <string.h>

void * xmalloc (size_t size);

void * xcalloc (size_t nmemb, size_t size);

void * xrealloc (void * ptr, size_t size);

char * xstrdup (const char *str);

#if !defined(HAVE_STPCPY)
/* Copy SRC to DEST, returning the address of the terminating '\0' in DEST.  */
static inline char * stpcpy (char *dest, const char * src) {
    register char *d = dest;
    register const char *s = src;

    do
	*d++ = *s;
    while (*s++ != '\0');
    return d - 1;
}
#endif

/* Memory allocation via macro defs to get meaningful locations from mtrace() */
#if defined(HAVE_MCHECK_H) && defined(__GNUC__)
#define	vmefail()	(fprintf(stderr, "virtual memory exhausted.\n"), exit(EXIT_FAILURE), NULL)
#define	xmalloc(_size) 		(malloc(_size) ? : vmefail())
#define	xcalloc(_nmemb, _size)	(calloc((_nmemb), (_size)) ? : vmefail())
#define	xrealloc(_ptr, _size)	(realloc((_ptr), (_size)) ? : vmefail())
#define xstrdup(_str)   (strcpy((malloc(strlen(_str)+1) ? : vmefail()), (_str)))
#else
#define	xmalloc(_size) 		malloc(_size)
#define	xcalloc(_nmemb, _size)	calloc((_nmemb), (_size))
#define	xrealloc(_ptr, _size)	realloc((_ptr), (_size))
#define	xstrdup(_str)	strdup(_str)
#endif  /* defined(HAVE_MCHECK_H) && defined(__GNUC__) */

#if defined(HAVE_SECURE_GETENV)
#define getenv(_s)	secure_getenv(_s)
#elif defined(HAVE___SECURE_GETENV)
#define	getenv(_s)	__secure_getenv(_s)
#endif

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x) 
#endif
#define UNUSED(x) x __attribute__((__unused__))
#define FORMAT(a, b, c) __attribute__((__format__ (a, b, c)))
#define NORETURN __attribute__((__noreturn__))

#include "popt.h"
