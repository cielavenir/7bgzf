#ifndef _LZMA_H_
#define _LZMA_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "../compat.h"
#include <stdio.h>
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
	#if (!defined(__GNUC__) && !defined(__clang__))
		#define LZMA_UNUSED
	#else
		#define LZMA_UNUSED __attribute__((unused))
	#endif
	#define LZMAIUnknownIMP22 HRESULT (WINAPI*QueryInterface)(void*, const GUID*, void**);u32 (WINAPI*AddRef)(void*);u32 (WINAPI*Release)(void*);
	#define LZMAIUnknownIMP23 HRESULT (WINAPI*QueryInterface)(void*, const GUID*, void**);u32 (WINAPI*AddRef)(void*);u32 (WINAPI*Release)(void*);
	// #define LZMAIUnknownIMP LZMAIUnknownIMP23
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
	#define LZMAIUnknownIMP22 HRESULT (WINAPI*QueryInterface)(void*, const GUID*, void**);u32 (WINAPI*AddRef)(void*);u32 (WINAPI*Release)(void*);void *Reserved1;void *Reserved2;
	#define LZMAIUnknownIMP23 HRESULT (WINAPI*QueryInterface)(void*, const GUID*, void**);u32 (WINAPI*AddRef)(void*);u32 (WINAPI*Release)(void*);
	// #ifdef Z7_USE_VIRTUAL_DESTRUCTOR_IN_IUNKNOWN
	// #define LZMAIUnknownIMP LZMAIUnknownIMP22
	// #else
	// #define LZMAIUnknownIMP LZMAIUnknownIMP23
	// #endif
#endif

int lzmaOpen7z();
bool lzma7zAlive();
int lzmaGet7zFileName(char* path, int siz);
int lzmaClose7z();

int lzmaShowInfos(FILE *out);
int lzmaGuessVersion();

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
	NCoderPropID_kMemUse,            // VT_UI8
	NCoderPropID_kAffinity,          // VT_UI8
};

enum{
	NMethodPropID_kID = 0,
	NMethodPropID_kName,
	NMethodPropID_kDecoder,
	NMethodPropID_kEncoder,
	NMethodPropID_kPackStreams,
	NMethodPropID_kUnpackStreams,
	NMethodPropID_kDescription,
	NMethodPropID_kDecoderIsAssigned,
	NMethodPropID_kEncoderIsAssigned,
	NMethodPropID_kDigestSize,
	NMethodPropID_kIsFilter,
};

// NArchive
enum{
	NHandlerPropID_kName = 0,        // VT_BSTR
	NHandlerPropID_kClassID,         // binary GUID in VT_BSTR
	NHandlerPropID_kExtension,       // VT_BSTR
	NHandlerPropID_kAddExtension,    // VT_BSTR
	NHandlerPropID_kUpdate,          // VT_BOOL
	NHandlerPropID_kKeepName,        // VT_BOOL
	NHandlerPropID_kSignature,       // binary in VT_BSTR
	NHandlerPropID_kMultiSignature,  // binary in VT_BSTR
	NHandlerPropID_kSignatureOffset, // VT_UI4
	NHandlerPropID_kAltStreams,      // VT_BOOL
	NHandlerPropID_kNtSecure,        // VT_BOOL
	NHandlerPropID_kFlags,           // VT_UI4
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
	LZMAIUnknownIMP22
	HRESULT (WINAPI*Read)(void* self, void *data, u32 size, u32 *processedSize);
	HRESULT (WINAPI*Seek)(void* self, s64 offset, u32 seekOrigin, u64 *newPosition);
} IInStream22_vt;

typedef struct{
	IInStream22_vt *vt;
} IInStream22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*Write)(void* self, const void *data, u32 size, u32 *processedSize);
	HRESULT (WINAPI*Seek)(void* self, s64 offset, u32 seekOrigin, u64 *newPosition);
	HRESULT (WINAPI*SetSize)(void* self, u64 newSize);
} IOutStream22_vt;

typedef struct{
	IOutStream22_vt *vt;
} IOutStream22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*SetRatioInfo)(void* self, const u64 *inSize, const u64 *outSize);
} ICompressProgressInfo22_vt;

typedef struct{
	ICompressProgressInfo22_vt *vt;
} ICompressProgressInfo22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*Code)(void* self, /*ISequentialInStream22_*/IInStream22_ *inStream, /*ISequentialOutStream22_*/IOutStream22_ *outStream, u64 *inSize, u64 *outSize, ICompressProgressInfo22_ *progress);
} ICompressCoder22_vt;

typedef struct{
	ICompressCoder22_vt *vt;
} ICompressCoder22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*GetNumMethods)(void* self, u32 *numMethods);
	HRESULT (WINAPI*GetProperty)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*CreateDecoder)(void* self, u32 index, const GUID *iid, void **coder);
	HRESULT (WINAPI*CreateEncoder)(void* self, u32 index, const GUID *iid, void **coder);
} ICompressCodecsInfo22_vt;

typedef struct{
	ICompressCodecsInfo22_vt *vt;
} ICompressCodecsInfo22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*SetCoderProperties)(void* self, const PROPID *propIDs, const PROPVARIANT *props, u32 numProps);
} ICompressSetCoderProperties22_vt;

typedef struct{
	ICompressSetCoderProperties22_vt *vt;
} ICompressSetCoderProperties22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*SetTotal)(void* self, const u64 *files, const u64 *bytes);
	HRESULT (WINAPI*SetCompleted)(void* self, const u64 *files, const u64 *bytes);
} IArchiveOpenCallback22_vt;

typedef struct{
	IArchiveOpenCallback22_vt *vt;
} IArchiveOpenCallback22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*GetProperty)(void* self, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*GetStream)(void* self, const wchar_t *name, IInStream22_ **inStream);
} IArchiveOpenVolumeCallback22_vt;

typedef struct{
	IArchiveOpenVolumeCallback22_vt *vt;
} IArchiveOpenVolumeCallback22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*SetTotal)(void* self, u64 total);
	HRESULT (WINAPI*SetCompleted)(void* self, const u64 *completedValue);
	HRESULT (WINAPI*GetStream)(void* self, u32 index, /*ISequentialOutStream22_*/IOutStream22_ **outStream, s32 askExtractMode);
	HRESULT (WINAPI*PrepareOperation)(void* self, s32 askExtractMode);
	HRESULT (WINAPI*SetOperationResult)(void* self, s32 opRes);
} IArchiveExtractCallback22_vt;

typedef struct{
	IArchiveExtractCallback22_vt *vt;
} IArchiveExtractCallback22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*Open)(void* self, IInStream22_ *stream, const u64 *maxCheckStartPosition, IArchiveOpenCallback22_ *openArchiveCallback);
	HRESULT (WINAPI*Close)(void* self);
	HRESULT (WINAPI*GetNumberOfItems)(void* self, u32 *numItems);
	HRESULT (WINAPI*GetProperty)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*Extract)(void* self, const u32* indices, u32 numItems, s32 testMode, IArchiveExtractCallback22_ *extractCallback);
	HRESULT (WINAPI*GetArchiveProperty)(void* self, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*GetNumberOfProperties)(void* self, u32 *numProperties);
	HRESULT (WINAPI*GetPropertyInfo)(void* self, u32 index, wchar_t **name, PROPID *propID, VARTYPE *varType);
	HRESULT (WINAPI*GetNumberOfArchiveProperties)(void* self, u32 *numProperties);
	HRESULT (WINAPI*GetArchivePropertyInfo)(void* self, u32 index, wchar_t **name, PROPID *propID, VARTYPE *varType);
} IInArchive22_vt;

typedef struct{
	IInArchive22_vt *vt;
} IInArchive22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*SetTotal)(void* self, u64 total);
	HRESULT (WINAPI*SetCompleted)(void* self, const u64 *completedValue);
	HRESULT (WINAPI*GetUpdateItemInfo)(void* self, u32 index, s32 *newData, s32 *newProps, u32 *indexInArchive);
	HRESULT (WINAPI*GetProperty)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*GetStream)(void* self, u32 index, /*ISequentialInStream22_*/IInStream22_ **inStream);
	HRESULT (WINAPI*SetOperationResult)(void* self, s32 operationResult);
} IArchiveUpdateCallback22_vt;

typedef struct{
	IArchiveUpdateCallback22_vt *vt;
} IArchiveUpdateCallback22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*SetProperties)(void* self, const wchar_t * const *names, const PROPVARIANT *values, u32 numProps);
} ISetProperties22_vt;

typedef struct{
	ISetProperties22_vt *vt;
} ISetProperties22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*CryptoGetTextPassword)(void* self, BSTR *password);
} ICryptoGetTextPassword22_vt;

typedef struct{
	ICryptoGetTextPassword22_vt *vt;
} ICryptoGetTextPassword22_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*CryptoGetTextPassword2)(void* self, s32 *passwordIsDefined, BSTR *password);
} ICryptoGetTextPassword222_vt;

typedef struct{
	ICryptoGetTextPassword222_vt *vt;
} ICryptoGetTextPassword222_;

typedef struct{
	LZMAIUnknownIMP22
	HRESULT (WINAPI*UpdateItems)(void* self, /*ISequentialOutStream22_*/IOutStream22_ *outStream, u32 numItems, IArchiveUpdateCallback22_ *updateCallback);
	HRESULT (WINAPI*GetFileTimeType)(void* self, u32 *type);
} IOutArchive22_vt;

typedef struct{
	IOutArchive22_vt *vt;
} IOutArchive22_;

typedef struct{
	LZMAIUnknownIMP22
	void    (WINAPI*Init)(void *self);
	void    (WINAPI*Update)(void *self, const void *data, u32 size);
	void    (WINAPI*Final)(void *self, u8 *digest);
	u32     (WINAPI*GetDigestSize)(void *self);
} IHasher22_vt;

typedef struct{
	IHasher22_vt *vt;
} IHasher22_;

typedef struct{
	LZMAIUnknownIMP22
	u32     (WINAPI*GetNumHashers)(void *self);
	HRESULT (WINAPI*GetHasherProp)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*CreateHasher)(void* self, u32 index, IHasher22_ **hasher);
} IHashers22_vt;

typedef struct{
	IHashers22_vt *vt;
} IHashers22_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*Read)(void* self, void *data, u32 size, u32 *processedSize);
	HRESULT (WINAPI*Seek)(void* self, s64 offset, u32 seekOrigin, u64 *newPosition);
} IInStream23_vt;

typedef struct{
	IInStream23_vt *vt;
} IInStream23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*Write)(void* self, const void *data, u32 size, u32 *processedSize);
	HRESULT (WINAPI*Seek)(void* self, s64 offset, u32 seekOrigin, u64 *newPosition);
	HRESULT (WINAPI*SetSize)(void* self, u64 newSize);
} IOutStream23_vt;

typedef struct{
	IOutStream23_vt *vt;
} IOutStream23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*SetRatioInfo)(void* self, const u64 *inSize, const u64 *outSize);
} ICompressProgressInfo23_vt;

typedef struct{
	ICompressProgressInfo23_vt *vt;
} ICompressProgressInfo23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*Code)(void* self, /*ISequentialInStream23_*/IInStream23_ *inStream, /*ISequentialOutStream23_*/IOutStream23_ *outStream, u64 *inSize, u64 *outSize, ICompressProgressInfo23_ *progress);
} ICompressCoder23_vt;

typedef struct{
	ICompressCoder23_vt *vt;
} ICompressCoder23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*GetNumMethods)(void* self, u32 *numMethods);
	HRESULT (WINAPI*GetProperty)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*CreateDecoder)(void* self, u32 index, const GUID *iid, void **coder);
	HRESULT (WINAPI*CreateEncoder)(void* self, u32 index, const GUID *iid, void **coder);
} ICompressCodecsInfo23_vt;

typedef struct{
	ICompressCodecsInfo23_vt *vt;
} ICompressCodecsInfo23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*SetCoderProperties)(void* self, const PROPID *propIDs, const PROPVARIANT *props, u32 numProps);
} ICompressSetCoderProperties23_vt;

typedef struct{
	ICompressSetCoderProperties23_vt *vt;
} ICompressSetCoderProperties23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*SetTotal)(void* self, const u64 *files, const u64 *bytes);
	HRESULT (WINAPI*SetCompleted)(void* self, const u64 *files, const u64 *bytes);
} IArchiveOpenCallback23_vt;

typedef struct{
	IArchiveOpenCallback23_vt *vt;
} IArchiveOpenCallback23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*GetProperty)(void* self, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*GetStream)(void* self, const wchar_t *name, IInStream23_ **inStream);
} IArchiveOpenVolumeCallback23_vt;

typedef struct{
	IArchiveOpenVolumeCallback23_vt *vt;
} IArchiveOpenVolumeCallback23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*SetTotal)(void* self, u64 total);
	HRESULT (WINAPI*SetCompleted)(void* self, const u64 *completedValue);
	HRESULT (WINAPI*GetStream)(void* self, u32 index, /*ISequentialOutStream23_*/IOutStream23_ **outStream, s32 askExtractMode);
	HRESULT (WINAPI*PrepareOperation)(void* self, s32 askExtractMode);
	HRESULT (WINAPI*SetOperationResult)(void* self, s32 opRes);
} IArchiveExtractCallback23_vt;

typedef struct{
	IArchiveExtractCallback23_vt *vt;
} IArchiveExtractCallback23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*Open)(void* self, IInStream23_ *stream, const u64 *maxCheckStartPosition, IArchiveOpenCallback23_ *openArchiveCallback);
	HRESULT (WINAPI*Close)(void* self);
	HRESULT (WINAPI*GetNumberOfItems)(void* self, u32 *numItems);
	HRESULT (WINAPI*GetProperty)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*Extract)(void* self, const u32* indices, u32 numItems, s32 testMode, IArchiveExtractCallback23_ *extractCallback);
	HRESULT (WINAPI*GetArchiveProperty)(void* self, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*GetNumberOfProperties)(void* self, u32 *numProperties);
	HRESULT (WINAPI*GetPropertyInfo)(void* self, u32 index, wchar_t **name, PROPID *propID, VARTYPE *varType);
	HRESULT (WINAPI*GetNumberOfArchiveProperties)(void* self, u32 *numProperties);
	HRESULT (WINAPI*GetArchivePropertyInfo)(void* self, u32 index, wchar_t **name, PROPID *propID, VARTYPE *varType);
} IInArchive23_vt;

typedef struct{
	IInArchive23_vt *vt;
} IInArchive23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*SetTotal)(void* self, u64 total);
	HRESULT (WINAPI*SetCompleted)(void* self, const u64 *completedValue);
	HRESULT (WINAPI*GetUpdateItemInfo)(void* self, u32 index, s32 *newData, s32 *newProps, u32 *indexInArchive);
	HRESULT (WINAPI*GetProperty)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*GetStream)(void* self, u32 index, /*ISequentialInStream23_*/IInStream23_ **inStream);
	HRESULT (WINAPI*SetOperationResult)(void* self, s32 operationResult);
} IArchiveUpdateCallback23_vt;

typedef struct{
	IArchiveUpdateCallback23_vt *vt;
} IArchiveUpdateCallback23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*SetProperties)(void* self, const wchar_t * const *names, const PROPVARIANT *values, u32 numProps);
} ISetProperties23_vt;

typedef struct{
	ISetProperties23_vt *vt;
} ISetProperties23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*CryptoGetTextPassword)(void* self, BSTR *password);
} ICryptoGetTextPassword23_vt;

typedef struct{
	ICryptoGetTextPassword23_vt *vt;
} ICryptoGetTextPassword23_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*CryptoGetTextPassword2)(void* self, s32 *passwordIsDefined, BSTR *password);
} ICryptoGetTextPassword223_vt;

typedef struct{
	ICryptoGetTextPassword223_vt *vt;
} ICryptoGetTextPassword223_;

typedef struct{
	LZMAIUnknownIMP23
	HRESULT (WINAPI*UpdateItems)(void* self, /*ISequentialOutStream23_*/IOutStream23_ *outStream, u32 numItems, IArchiveUpdateCallback23_ *updateCallback);
	HRESULT (WINAPI*GetFileTimeType)(void* self, u32 *type);
} IOutArchive23_vt;

typedef struct{
	IOutArchive23_vt *vt;
} IOutArchive23_;

typedef struct{
	LZMAIUnknownIMP23
	void    (WINAPI*Init)(void *self);
	void    (WINAPI*Update)(void *self, const void *data, u32 size);
	void    (WINAPI*Final)(void *self, u8 *digest);
	u32     (WINAPI*GetDigestSize)(void *self);
} IHasher23_vt;

typedef struct{
	IHasher23_vt *vt;
} IHasher23_;

typedef struct{
	LZMAIUnknownIMP23
	u32     (WINAPI*GetNumHashers)(void *self);
	HRESULT (WINAPI*GetHasherProp)(void* self, u32 index, PROPID propID, PROPVARIANT *value);
	HRESULT (WINAPI*CreateHasher)(void* self, u32 index, IHasher23_ **hasher);
} IHashers23_vt;

typedef struct{
	IHashers23_vt *vt;
} IHashers23_;

/// Some special derivatives ///

typedef struct{
	IInStream22_vt *vt;
	u32 refs;
	FILE *f;
} SInStreamFile22;

bool WINAPI MakeSInStreamFile22(SInStreamFile22 *self, const char *fname);

typedef struct{
	IInStream22_vt *vt;
	u32 refs;
	memstream *m;
} SInStreamMem22;

bool MakeSInStreamMem22(SInStreamMem22 *self, void *p, const unsigned int size);

typedef struct{
	IInStream22_vt *vt;
	u32 refs;
	void *h;
	tRead pRead;
	tClose pClose;
	tSeek pSeek;
	tTell pTell;
} SInStreamGeneric22;

bool MakeSInStreamGeneric22(SInStreamGeneric22 *self, void *h, tRead pRead, tClose pClose, tSeek pSeek, tTell pTell);

typedef struct{
	IOutStream22_vt *vt;
	u32 refs;
	FILE *f;
} SOutStreamFile22;

bool MakeSOutStreamFile22(SOutStreamFile22 *self, const char *fname, bool readable);

typedef struct{
	IOutStream22_vt *vt;
	u32 refs;
	memstream *m;
} SSequentialOutStreamMem22;

bool MakeSSequentialOutStreamMem22(SSequentialOutStreamMem22 *self, void *p, const unsigned int size);

typedef struct{
	IOutStream22_vt *vt;
	u32 refs;
	void *h;
	tWrite pWrite;
	tClose pClose;
} SSequentialOutStreamGeneric22;

bool MakeSSequentialOutStreamGeneric22(SSequentialOutStreamGeneric22 *self, void *h, tWrite pWrite, tClose pClose);

typedef struct{
	ICryptoGetTextPassword22_vt *vt;
	u32 refs;
	char *password;
} SCryptoGetTextPasswordFixed22;

//password can be null
bool MakeSCryptoGetTextPasswordFixed22(SCryptoGetTextPasswordFixed22 *self, const char *password);

typedef struct{
	ICryptoGetTextPassword222_vt *vt;
	u32 refs;
	char *password;
} SCryptoGetTextPassword2Fixed22;

//password can be null
bool MakeSCryptoGetTextPassword2Fixed22(SCryptoGetTextPassword2Fixed22 *self, const char *password);

typedef struct{
	IArchiveOpenVolumeCallback22_vt *vt;
	u32 refs;
	char *fname;
} SArchiveOpenVolumeCallback22;

//fname can be null
bool MakeSArchiveOpenVolumeCallback22(SArchiveOpenVolumeCallback22 *self, const char *fname);

typedef struct{
	IArchiveOpenCallback22_vt *vt;
	u32 refs;
	SCryptoGetTextPasswordFixed22 setpassword;
	SArchiveOpenVolumeCallback22 openvolume;
} SArchiveOpenCallbackPassword22;

//password/fname can be null
bool MakeSArchiveOpenCallbackPassword22(SArchiveOpenCallbackPassword22 *self, const char *password, const char *fname);

typedef struct{
	IArchiveExtractCallback22_vt *vt;
	u32 refs;
	SCryptoGetTextPasswordFixed22 setpassword;
	IInArchive22_ *archiver;
	u32 lastIndex;
} SArchiveExtractCallbackBare22;

//password can be null
bool MakeSArchiveExtractCallbackBare22(SArchiveExtractCallbackBare22 *self, IInArchive22_ *archiver, const char *password);

typedef struct{
	IArchiveUpdateCallback22_vt *vt;
	u32 refs;
	SCryptoGetTextPassword2Fixed22 setpassword;
	IOutArchive22_ *archiver;
	u32 lastIndex;
} SArchiveUpdateCallbackBare22;

bool MakeSArchiveUpdateCallbackBare22(SArchiveUpdateCallbackBare22 *self, IOutArchive22_ *archiver, const char *password);

typedef struct{
	IInStream23_vt *vt;
	u32 refs;
	FILE *f;
} SInStreamFile23;

bool WINAPI MakeSInStreamFile23(SInStreamFile23 *self, const char *fname);

typedef struct{
	IInStream23_vt *vt;
	u32 refs;
	memstream *m;
} SInStreamMem23;

bool MakeSInStreamMem23(SInStreamMem23 *self, void *p, const unsigned int size);

typedef struct{
	IInStream23_vt *vt;
	u32 refs;
	void *h;
	tRead pRead;
	tClose pClose;
	tSeek pSeek;
	tTell pTell;
} SInStreamGeneric23;

bool MakeSInStreamGeneric23(SInStreamGeneric23 *self, void *h, tRead pRead, tClose pClose, tSeek pSeek, tTell pTell);

typedef struct{
	IOutStream23_vt *vt;
	u32 refs;
	FILE *f;
} SOutStreamFile23;

bool MakeSOutStreamFile23(SOutStreamFile23 *self, const char *fname, bool readable);

typedef struct{
	IOutStream23_vt *vt;
	u32 refs;
	memstream *m;
} SSequentialOutStreamMem23;

bool MakeSSequentialOutStreamMem23(SSequentialOutStreamMem23 *self, void *p, const unsigned int size);

typedef struct{
	IOutStream23_vt *vt;
	u32 refs;
	void *h;
	tWrite pWrite;
	tClose pClose;
} SSequentialOutStreamGeneric23;

bool MakeSSequentialOutStreamGeneric23(SSequentialOutStreamGeneric23 *self, void *h, tWrite pWrite, tClose pClose);

typedef struct{
	ICryptoGetTextPassword23_vt *vt;
	u32 refs;
	char *password;
} SCryptoGetTextPasswordFixed23;

//password can be null
bool MakeSCryptoGetTextPasswordFixed23(SCryptoGetTextPasswordFixed23 *self, const char *password);

typedef struct{
	ICryptoGetTextPassword223_vt *vt;
	u32 refs;
	char *password;
} SCryptoGetTextPassword2Fixed23;

//password can be null
bool MakeSCryptoGetTextPassword2Fixed23(SCryptoGetTextPassword2Fixed23 *self, const char *password);

typedef struct{
	IArchiveOpenVolumeCallback23_vt *vt;
	u32 refs;
	char *fname;
} SArchiveOpenVolumeCallback23;

//fname can be null
bool MakeSArchiveOpenVolumeCallback23(SArchiveOpenVolumeCallback23 *self, const char *fname);

typedef struct{
	IArchiveOpenCallback23_vt *vt;
	u32 refs;
	SCryptoGetTextPasswordFixed23 setpassword;
	SArchiveOpenVolumeCallback23 openvolume;
} SArchiveOpenCallbackPassword23;

//password/fname can be null
bool MakeSArchiveOpenCallbackPassword23(SArchiveOpenCallbackPassword23 *self, const char *password, const char *fname);

typedef struct{
	IArchiveExtractCallback23_vt *vt;
	u32 refs;
	SCryptoGetTextPasswordFixed23 setpassword;
	IInArchive23_ *archiver;
	u32 lastIndex;
} SArchiveExtractCallbackBare23;

//password can be null
bool MakeSArchiveExtractCallbackBare23(SArchiveExtractCallbackBare23 *self, IInArchive23_ *archiver, const char *password);

typedef struct{
	IArchiveUpdateCallback23_vt *vt;
	u32 refs;
	SCryptoGetTextPassword2Fixed23 setpassword;
	IOutArchive23_ *archiver;
	u32 lastIndex;
} SArchiveUpdateCallbackBare23;

bool MakeSArchiveUpdateCallbackBare23(SArchiveUpdateCallbackBare23 *self, IOutArchive23_ *archiver, const char *password);

int lzmaLoadExternalCodecs();
int lzmaUnloadExternalCodecs();

unsigned long long FileTimeToUTC(const FILETIME in);
FILETIME UTCToFileTime(const unsigned long long UTC);

#ifdef __cplusplus
}
#endif

#endif
