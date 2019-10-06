#ifndef _LZMA_H_
#define _LZMA_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "../compat.h"
#include <stdio.h>
#include <stdlib.h> // getenv
#include <stdbool.h>

#include "memstream.h"

#ifdef FEOS
typedef long fpos_t;
#endif
//from BSD libc
typedef int (*tRead)(void *, char *, int);
typedef int (*tWrite)(void *, const char *, int);
typedef int (*tSeek)(void *, long long, int);
typedef long long (*tTell)(void *);
typedef int (*tClose)(void *);

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	#include <propidl.h> // PropVariantClear
	#include <oleauto.h> // SysAllocStr etc
	#define LZMA_UNUSED
	#define LZMAIUnknownIMP HRESULT (WINAPI*QueryInterface)(void*, const GUID*, void**);u32 (WINAPI*AddRef)(void*);u32 (WINAPI*Release)(void*);
#else
	#include "lzma_windows.h"
	#define LZMA_UNUSED __attribute__((unused))
	HRESULT PropVariantClear(PROPVARIANT *pvar);
	BSTR SysAllocString(const OLECHAR *str);
	BSTR SysAllocStringLen(const OLECHAR *str,u32 len);
	void SysFreeString(BSTR str);
	u32 SysStringLen(BSTR str);
	// Reserved1 and Reserved2:
	// CPP/Common/MyWindows.h (cf: https://forum.lazarus.freepascal.org/index.php/topic,42701.msg298820.html#msg298820)
	#define LZMAIUnknownIMP HRESULT (WINAPI*QueryInterface)(void*, const GUID*, void**);u32 (WINAPI*AddRef)(void*);u32 (WINAPI*Release)(void*);void *Reserved1;void *Reserved2;
#endif

int lzmaOpen7z();
bool lzma7zAlive();
int lzmaClose7z();

/*
Archive API
arctype (as of 7-zip 19.00):

  01 Zip
  02 BZip2
  03 Rar
  04 Arj
  05 Z
  06 Lzh
  07 7z
  08 Cab
  09 Nsis
  0A lzma
  0B lzma86
  0C xz
  0D ppmd

  C6 COFF
  C7 Ext
  C8 VMDK
  C9 VDI
  CA Qcow
  CB GPT
  CC Rar5
  CD IHex
  CE Hxs
  CF TE
  D0 UEFIc
  D1 UEFIs
  D2 SquashFS
  D3 CramFS
  D4 APM
  D5 Mslz
  D6 Flv
  D7 Swf
  D8 Swfc
  D9 Ntfs
  DA Fat
  DB Mbr
  DC Vhd
  DD Pe
  DE Elf
  DF Mach-O
  E0 Udf
  E1 Xar
  E2 Mub
  E3 Hfs
  E4 Dmg
  E5 Compound
  E6 Wim
  E7 Iso
  E8 
  E9 Chm
  EA Split
  EB Rpm
  EC Deb
  ED Cpio
  EE Tar
  EF GZip
*/
int lzmaCreateArchiver(void **archiver,unsigned char arctype,int encode,int level);
int lzmaDestroyArchiver(void **archiver,int encode);
//fname can be used to assist opening multi-volume archive
int lzmaOpenArchive(void *archiver,void *reader,const char *password,const char *fname);
int lzmaCloseArchive(void *archiver);
int lzmaGetArchiveFileNum(void *archiver,unsigned int *numItems);
int lzmaGetArchiveFileProperty(void *archiver,unsigned int index,int kpid,PROPVARIANT *prop);
int lzmaExtractArchive(void *archiver,const unsigned int* indices, unsigned int numItems, int testMode, void *callback);
int lzmaUpdateArchive(void *archiver,void *writer,u32 numItems,void *callback);

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
int lzmaCodeOneshot(void *coder, const unsigned char *in, size_t isize, unsigned char *out, size_t *osize);
int lzmaCodeCallback(void *coder, void *hin, tRead pRead_in, tClose pClose_in, void *hout, tWrite pWrite_out, tClose pClose_out);

/// 7z.so interface extracted from LZMA SDK CPP/7zip ///

enum{
	NCoderPropID_kDefaultProp = 0,
	NCoderPropID_kDictionarySize,    // VT_UI4
	NCoderPropID_kUsedMemorySize,    // VT_UI4
	NCoderPropID_kOrder,             // VT_UI4
	NCoderPropID_kBlockSize,         // VT_UI4 or VT_UI8
	NCoderPropID_kPosStateBits,      // VT_UI4
	NCoderPropID_kLitContextBits,    // VT_UI4
	NCoderPropID_kLitPosBits,        // VT_UI4
	NCoderPropID_kNumFastBytes,      // VT_UI4
	NCoderPropID_kMatchFinder,       // VT_BSTR
	NCoderPropID_kMatchFinderCycles, // VT_UI4
	NCoderPropID_kNumPasses,         // VT_UI4
	NCoderPropID_kAlgorithm,         // VT_UI4
	NCoderPropID_kNumThreads,        // VT_UI4
	NCoderPropID_kEndMarker,         // VT_BOOL
	NCoderPropID_kLevel,             // VT_UI4
	NCoderPropID_kReduceSize,        // VT_UI8 : it's estimated size of largest data stream that will be compressed
					  //   encoder can use this value to reduce dictionary size and allocate data buffers

	NCoderPropID_kExpectedDataSize,  // VT_UI8 : for ICompressSetCoderPropertiesOpt :
					  //   it's estimated size of current data stream
					  //   real data size can differ from that size
					  //   encoder can use this value to optimize encoder initialization

	NCoderPropID_kBlockSize2,        // VT_UI4 or VT_UI8
	NCoderPropID_kCheckSize,         // VT_UI4 : size of digest in bytes
	NCoderPropID_kFilter,            // VT_BSTR
	NCoderPropID_kMemUse             // VT_UI8
};

enum
{
	kpidNoProperty = 0,
	kpidMainSubfile,
	kpidHandlerItemIndex,
	kpidPath,
	kpidName,
	kpidExtension,
	kpidIsDir,
	kpidSize,
	kpidPackSize,
	kpidAttrib,
	kpidCTime,
	kpidATime,
	kpidMTime,
	kpidSolid,
	kpidCommented,
	kpidEncrypted,
	kpidSplitBefore,
	kpidSplitAfter,
	kpidDictionarySize,
	kpidCRC,
	kpidType,
	kpidIsAnti,
	kpidMethod,
	kpidHostOS,
	kpidFileSystem,
	kpidUser,
	kpidGroup,
	kpidBlock,
	kpidComment,
	kpidPosition,
	kpidPrefix,
	kpidNumSubDirs,
	kpidNumSubFiles,
	kpidUnpackVer,
	kpidVolume,
	kpidIsVolume,
	kpidOffset,
	kpidLinks,
	kpidNumBlocks,
	kpidNumVolumes,
	kpidTimeType,
	kpidBit64,
	kpidBigEndian,
	kpidCpu,
	kpidPhySize,
	kpidHeadersSize,
	kpidChecksum,
	kpidCharacts,
	kpidVa,
	kpidId,
	kpidShortName,
	kpidCreatorApp,
	kpidSectorSize,
	kpidPosixAttrib,
	kpidSymLink,
	kpidError,
	kpidTotalSize,
	kpidFreeSpace,
	kpidClusterSize,
	kpidVolumeName,
	kpidLocalName,
	kpidProvider,
	kpidNtSecure,
	kpidIsAltStream,
	kpidIsAux,
	kpidIsDeleted,
	kpidIsTree,
	kpidSha1,
	kpidSha256,
	kpidErrorType,
	kpidNumErrors,
	kpidErrorFlags,
	kpidWarningFlags,
	kpidWarning,
	kpidNumStreams,
	kpidNumAltStreams,
	kpidAltStreamsSize,
	kpidVirtualSize,
	kpidUnpackSize,
	kpidTotalPhySize,
	kpidVolumeIndex,
	kpidSubType,
	kpidShortComment,
	kpidCodePage,
	kpidIsNotArcType,
	kpidPhySizeCantBeDetected,
	kpidZerosTailIsAllowed,
	kpidTailSize,
	kpidEmbeddedStubSize,
	kpidNtReparse,
	kpidHardLink,
	kpidINode,
	kpidStreamId,
	kpidReadOnly,
	kpidOutName,
	kpidCopyLink,

	kpid_NUM_DEFINED,

	kpidUserDefined = 0x10000
};

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*Read)(void* self, void *data, u32 size, u32 *processedSize);
	HRESULT (WINAPI*Seek)(void* self, s64 offset, u32 seekOrigin, u64 *newPosition);
} IInStream_vt;

typedef struct{
	IInStream_vt *vt;
} IInStream_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*Write)(void* self, const void *data, u32 size, u32 *processedSize);
	HRESULT (WINAPI*Seek)(void* self, s64 offset, u32 seekOrigin, u64 *newPosition);
	HRESULT (WINAPI*SetSize)(void* self, u64 newSize);
} IOutStream_vt;

typedef struct{
	IOutStream_vt *vt;
} IOutStream_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*SetRatioInfo)(void* self, const u64 *inSize, const u64 *outSize);
} ICompressProgressInfo_vt;

typedef struct{
	ICompressProgressInfo_vt *vt;
} ICompressProgressInfo_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*Code)(void* self, /*ISequentialInStream_*/IInStream_ *inStream, /*ISequentialOutStream_*/IOutStream_ *outStream, u64 *inSize, u64 *outSize, ICompressProgressInfo_ *progress);
} ICompressCoder_vt;

typedef struct{
	ICompressCoder_vt *vt;
} ICompressCoder_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*GetNumMethods)(void* self, u32 *numMethods);
	HRESULT (WINAPI*GetProperty)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*CreateDecoder)(void* self, u32 index, const GUID *iid, void **coder);
	HRESULT (WINAPI*CreateEncoder)(void* self, u32 index, const GUID *iid, void **coder);
} ICompressCodecsInfo_vt;

typedef struct{
	ICompressCodecsInfo_vt *vt;
} ICompressCodecsInfo_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*SetCoderProperties)(void* self, const PROPID *propIDs, const PROPVARIANT *props, u32 numProps);
} ICompressSetCoderProperties_vt;

typedef struct{
	ICompressSetCoderProperties_vt *vt;
} ICompressSetCoderProperties_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*SetTotal)(void* self, const u64 *files, const u64 *bytes);
	HRESULT (WINAPI*SetCompleted)(void* self, const u64 *files, const u64 *bytes);
} IArchiveOpenCallback_vt;

typedef struct{
	IArchiveOpenCallback_vt *vt;
} IArchiveOpenCallback_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*GetProperty)(void* self, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*GetStream)(void* self, const wchar_t *name, IInStream_ **inStream);
} IArchiveOpenVolumeCallback_vt;

typedef struct{
	IArchiveOpenVolumeCallback_vt *vt;
} IArchiveOpenVolumeCallback_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*SetTotal)(void* self, u64 total);
	HRESULT (WINAPI*SetCompleted)(void* self, const u64 *completedValue);
	HRESULT (WINAPI*GetStream)(void* self, u32 index, /*ISequentialOutStream_*/IOutStream_ **outStream, s32 askExtractMode);
	HRESULT (WINAPI*PrepareOperation)(void* self, s32 askExtractMode);
	HRESULT (WINAPI*SetOperationResult)(void* self, s32 opRes);
} IArchiveExtractCallback_vt;

typedef struct{
	IArchiveExtractCallback_vt *vt;
} IArchiveExtractCallback_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*Open)(void* self, IInStream_ *stream, const u64 *maxCheckStartPosition, IArchiveOpenCallback_ *openArchiveCallback);
	HRESULT (WINAPI*Close)(void* self);
	HRESULT (WINAPI*GetNumberOfItems)(void* self, u32 *numItems);
	HRESULT (WINAPI*GetProperty)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*Extract)(void* self, const u32* indices, u32 numItems, s32 testMode, IArchiveExtractCallback_ *extractCallback);
	HRESULT (WINAPI*GetArchiveProperty)(void* self, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*GetNumberOfProperties)(void* self, u32 *numProperties);
	HRESULT (WINAPI*GetPropertyInfo)(void* self, u32 index, wchar_t **name, PROPID *propID, VARTYPE *varType);
	HRESULT (WINAPI*GetNumberOfArchiveProperties)(void* self, u32 *numProperties);
	HRESULT (WINAPI*GetArchivePropertyInfo)(void* self, u32 index, wchar_t **name, PROPID *propID, VARTYPE *varType);
} IInArchive_vt;

typedef struct{
	IInArchive_vt *vt;
} IInArchive_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*SetTotal)(void* self, u64 total);
	HRESULT (WINAPI*SetCompleted)(void* self, const u64 *completedValue);
	HRESULT (WINAPI*GetUpdateItemInfo)(void* self, u32 index, s32 *newData, s32 *newProps, u32 *indexInArchive);
	HRESULT (WINAPI*GetProperty)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*GetStream)(void* self, u32 index, /*ISequentialInStream_*/IInStream_ **inStream);
	HRESULT (WINAPI*SetOperationResult)(void* self, s32 operationResult);
} IArchiveUpdateCallback_vt;

typedef struct{
	IArchiveUpdateCallback_vt *vt;
} IArchiveUpdateCallback_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*SetProperties)(void* self, const wchar_t * const *names, const PROPVARIANT *values, u32 numProps);
} ISetProperties_vt;

typedef struct{
	ISetProperties_vt *vt;
} ISetProperties_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*CryptoGetTextPassword)(void* self, BSTR *password);
} ICryptoGetTextPassword_vt;

typedef struct{
	ICryptoGetTextPassword_vt *vt;
} ICryptoGetTextPassword_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*CryptoGetTextPassword2)(void* self, s32 *passwordIsDefined, BSTR *password);
} ICryptoGetTextPassword2_vt;

typedef struct{
	ICryptoGetTextPassword2_vt *vt;
} ICryptoGetTextPassword2_;

typedef struct{
	LZMAIUnknownIMP
	HRESULT (WINAPI*UpdateItems)(void* self, /*ISequentialOutStream_*/IOutStream_ *outStream, u32 numItems, IArchiveUpdateCallback_ *updateCallback);
	HRESULT (WINAPI*GetFileTimeType)(void* self, u32 *type);
} IOutArchive_vt;

typedef struct{
	IOutArchive_vt *vt;
} IOutArchive_;

/// Some special derivatives ///

typedef struct{
	IInStream_vt *vt;
	u32 refs;
	FILE *f;
} SInStreamFile;

bool WINAPI MakeSInStreamFile(SInStreamFile *self, const char *fname);

typedef struct{
	IInStream_vt *vt;
	u32 refs;
	memstream *m;
} SInStreamMem;

bool MakeSInStreamMem(SInStreamMem *self, void *p, const unsigned int size);

typedef struct{
	IInStream_vt *vt;
	u32 refs;
	void *h;
	tRead pRead;
	tClose pClose;
	tSeek pSeek;
	tTell pTell;
} SInStreamGeneric;

bool MakeSInStreamGeneric(SInStreamGeneric *self, void *h, tRead pRead, tClose pClose, tSeek pSeek, tTell pTell);

typedef struct{
	IOutStream_vt *vt;
	u32 refs;
	FILE *f;
} SOutStreamFile;

bool MakeSOutStreamFile(SOutStreamFile *self, const char *fname, bool readable);

typedef struct{
	IOutStream_vt *vt;
	u32 refs;
	memstream *m;
} SSequentialOutStreamMem;

bool MakeSSequentialOutStreamMem(SSequentialOutStreamMem *self, void *p, const unsigned int size);

typedef struct{
	IOutStream_vt *vt;
	u32 refs;
	void *h;
	tWrite pWrite;
	tClose pClose;
} SSequentialOutStreamGeneric;

bool MakeSSequentialOutStreamGeneric(SSequentialOutStreamGeneric *self, void *h, tWrite pWrite, tClose pClose);

typedef struct{
	ICryptoGetTextPassword_vt *vt;
	u32 refs;
	char *password;
} SCryptoGetTextPasswordFixed;

//password can be null
bool MakeSCryptoGetTextPasswordFixed(SCryptoGetTextPasswordFixed *self, const char *password);

typedef struct{
	ICryptoGetTextPassword2_vt *vt;
	u32 refs;
	char *password;
} SCryptoGetTextPassword2Fixed;

//password can be null
bool MakeSCryptoGetTextPassword2Fixed(SCryptoGetTextPassword2Fixed *self, const char *password);

typedef struct{
	IArchiveOpenVolumeCallback_vt *vt;
	u32 refs;
	char *fname;
} SArchiveOpenVolumeCallback;

//fname can be null
bool MakeSArchiveOpenVolumeCallback(SArchiveOpenVolumeCallback *self, const char *fname);

typedef struct{
	IArchiveOpenCallback_vt *vt;
	u32 refs;
	SCryptoGetTextPasswordFixed setpassword;
	SArchiveOpenVolumeCallback openvolume;
} SArchiveOpenCallbackPassword;

//password/fname can be null
bool MakeSArchiveOpenCallbackPassword(SArchiveOpenCallbackPassword *self, const char *password, const char *fname);

typedef struct{
	IArchiveExtractCallback_vt *vt;
	u32 refs;
	SCryptoGetTextPasswordFixed setpassword;
	IInArchive_ *archiver;
	u32 lastIndex;
} SArchiveExtractCallbackBare;

//password can be null
bool MakeSArchiveExtractCallbackBare(SArchiveExtractCallbackBare *self, IInArchive_ *archiver, const char *password);

typedef struct{
	IArchiveUpdateCallback_vt *vt;
	u32 refs;
	SCryptoGetTextPassword2Fixed setpassword;
	IOutArchive_ *archiver;
	u32 lastIndex;
} SArchiveUpdateCallbackBare;

bool MakeSArchiveUpdateCallbackBare(SArchiveUpdateCallbackBare *self, IOutArchive_ *archiver, const char *password);

int lzmaLoadUnrar();
int lzmaUnloadUnrar();

unsigned long long FileTimeToUTC(const FILETIME in);
FILETIME UTCToFileTime(const unsigned long long UTC);

#ifdef __cplusplus
}
#endif

#endif
