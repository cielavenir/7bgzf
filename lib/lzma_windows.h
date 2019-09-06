/*
	windows.h - main header file for the Win32 API

	Written by Anders Norlander <anorland@hem2.passagen.se>

	This file is part of a free library for the Win32 API.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/
#ifndef _WINDOWS_H
#define _WINDOWS_H

#include <stdarg.h>
#include <stddef.h>

#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 Int64;
typedef unsigned __int64 UInt64;
#define UINT64_CONST(n) n
#else
typedef long long int Int64;
typedef unsigned long long int UInt64;
#define UINT64_CONST(n) n ## ULL
#endif

/* BEGIN #include <windef.h> */

#ifndef CONST
#define CONST const
#endif

#undef MAX_PATH
#define MAX_PATH 4096  /* Linux : 4096  - Windows : 260 */

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define WINAPI 

/* BEGIN #include <winnt.h> */
/* BEGIN <winerror.h> */
#define NO_ERROR                    0L
#define ERROR_ALREADY_EXISTS        EEXIST
#define ERROR_FILE_EXISTS           EEXIST
#define ERROR_INVALID_HANDLE        EBADF
#define ERROR_PATH_NOT_FOUND        ENOENT
#define ERROR_DISK_FULL             ENOSPC
#define ERROR_NO_MORE_FILES         0x100123 // FIXME

#define S_OK ((HRESULT)0x00000000L)
#define S_FALSE ((HRESULT)0x00000001L)
#define E_INVALIDARG ((HRESULT)0x80070057L)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#define E_ABORT ((HRESULT)0x80004004L)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define STG_E_INVALIDFUNCTION ((HRESULT)0x80030001L)
#define SUCCEEDED(Status) ((HRESULT)(Status) >= 0)
#define FAILED(Status) ((HRESULT)(Status)<0)

#ifndef VOID
#define VOID void
#endif
typedef char CHAR;
typedef short SHORT;
typedef int LONG;
typedef int INT;
typedef CHAR CCHAR;
typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int ULONG;
typedef unsigned int UINT;

typedef unsigned short WORD;
typedef unsigned int DWORD;

typedef LONG HRESULT;
typedef ULONG PROPID;
typedef LONG SCODE;

typedef wchar_t WCHAR;
typedef WCHAR *PWCHAR,*LPWCH,*PWCH,*NWPSTR,*LPWSTR,*PWSTR;
typedef CONST WCHAR *LPCWCH,*PCWCH,*LPCWSTR,*PCWSTR;
typedef CHAR *PCHAR,*LPCH,*PCH,*NPSTR,*LPSTR,*PSTR;
typedef CONST CHAR *LPCCH,*PCSTR,*LPCSTR;

typedef Int64 LONGLONG;
typedef UInt64 ULONGLONG;
typedef struct _LARGE_INTEGER { LONGLONG QuadPart; } LARGE_INTEGER;
typedef struct _ULARGE_INTEGER { ULONGLONG QuadPart; } ULARGE_INTEGER;
typedef WORD VARTYPE;
typedef WORD PROPVAR_PAD1;
typedef WORD PROPVAR_PAD2;
typedef WORD PROPVAR_PAD3;
typedef struct _FILETIME
{
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME;
typedef short VARIANT_BOOL;
typedef WCHAR OLECHAR;
typedef OLECHAR *BSTR;

#define VARIANT_TRUE ((VARIANT_BOOL)-1)
#define VARIANT_FALSE ((VARIANT_BOOL)0)

typedef struct{
  VARTYPE vt;
  PROPVAR_PAD1 wReserved1;
  PROPVAR_PAD2 wReserved2;
  PROPVAR_PAD3 wReserved3;
  union
  {
    CHAR cVal;
    UCHAR bVal;
    SHORT iVal;
    USHORT uiVal;
    LONG lVal;
    ULONG ulVal;
    INT intVal;
    UINT uintVal;
    LARGE_INTEGER hVal;
    ULARGE_INTEGER uhVal;
    VARIANT_BOOL boolVal;
    SCODE scode;
    FILETIME filetime;
    BSTR bstrVal;
  };
} PROPVARIANT;

enum VARENUM
{
  VT_EMPTY = 0,
  VT_NULL = 1,
  VT_I2 = 2,
  VT_I4 = 3,
  VT_R4 = 4,
  VT_R8 = 5,
  VT_CY = 6,
  VT_DATE = 7,
  VT_BSTR = 8,
  VT_DISPATCH = 9,
  VT_ERROR = 10,
  VT_BOOL = 11,
  VT_VARIANT = 12,
  VT_UNKNOWN = 13,
  VT_DECIMAL = 14,
  VT_I1 = 16,
  VT_UI1 = 17,
  VT_UI2 = 18,
  VT_UI4 = 19,
  VT_I8 = 20,
  VT_UI8 = 21,
  VT_INT = 22,
  VT_UINT = 23,
  VT_VOID = 24,
  VT_HRESULT = 25,
  VT_FILETIME = 64
};

typedef struct _GUID {
	unsigned int  Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char  Data4[8];
} GUID, *REFGUID, *LPGUID, IID, *REFIID, *LPIID;

#ifdef UNICODE
/*
 * P7ZIP_TEXT is a private macro whose specific use is to force the expansion of a
 * macro passed as an argument to the macro TEXT.  DO NOT use this
 * macro within your programs.  It's name and function could change without
 * notice.
 */
#define P7ZIP_TEXT(q) L##q
#else
#define P7ZIP_TEXT(q) q
#endif
/*
 * UNICODE a constant string when UNICODE is defined, else returns the string
 * unmodified.
 * The corresponding macros  _TEXT() and _T() for mapping _UNICODE strings
 * passed to C runtime functions are defined in mingw/tchar.h
 */
#define TEXT(q) P7ZIP_TEXT(q)    

/* BEGIN #include <basetsd.h> */
#ifndef __int64
#define __int64 long long
#endif
typedef unsigned __int64 UINT64;
typedef __int64 INT64;
/* END #include <basetsd.h> */

#define FILE_ATTRIBUTE_READONLY             1
#define FILE_ATTRIBUTE_HIDDEN               2
#define FILE_ATTRIBUTE_SYSTEM               4
#define FILE_ATTRIBUTE_DIRECTORY           16
#define FILE_ATTRIBUTE_ARCHIVE             32
#define FILE_ATTRIBUTE_DEVICE              64
#define FILE_ATTRIBUTE_NORMAL             128
#define FILE_ATTRIBUTE_TEMPORARY          256
#define FILE_ATTRIBUTE_SPARSE_FILE        512
#define FILE_ATTRIBUTE_REPARSE_POINT     1024
#define FILE_ATTRIBUTE_COMPRESSED        2048
#define FILE_ATTRIBUTE_OFFLINE          0x1000
#define FILE_ATTRIBUTE_ENCRYPTED        0x4000
#define FILE_ATTRIBUTE_UNIX_EXTENSION   0x8000   /* trick for Unix */

/* END   <winerror.h> */

/* END #include <winnt.h> */

/* END #include <windef.h> */

/* BEGIN #include <winnls.h> */

#define CP_ACP   0
#define CP_OEMCP 1
#define CP_UTF8  65001

/* #include <unknwn.h> */
#ifdef ENV_HAVE_GCCVISIBILITYPATCH
    #define DLLEXPORT __attribute__ ((visibility("default")))
#else
    #define DLLEXPORT
#endif

#ifdef __cplusplus
#define STDAPI extern "C" DLLEXPORT HRESULT
#else
#define STDAPI extern DLLEXPORT HRESULT
#endif  /* __cplusplus */ 

/* END #include <ole2.h> */

#endif
