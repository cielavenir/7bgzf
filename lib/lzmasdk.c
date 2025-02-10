// LZMA SDK C interface
// basic idea from:
// https://gist.github.com/harvimt/9461046
// https://github.com/harvimt/pylib7zip

// STDMETHOD is virtual __stdcall (or __cdecl), so it can be called from C if the interface is properly aligned.

#include "lzma.h"
#include "scv.h"
#include <string.h>
#include <stdlib.h> // getenv
#include <dirent.h>

#ifndef wchar_t
#include <wchar.h>
#endif

#define MAKE(STRUCT_NAME, ...) Make ## __VA_ARGS__ ## STRUCT_NAME

#ifdef __cplusplus
extern "C"{
#define EXTERN extern
#else
#define EXTERN
#endif

static inline unsigned int read32(const void *p){
	const unsigned char *x=(const unsigned char*)p;
	return x[0]|(x[1]<<8)|(x[2]<<16)|((unsigned int)x[3]<<24);
}

static inline void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}

static inline int max32(const int x,const int y){
	return y ^ ( (x ^ y) & ( (y - x) >> 31 ) );
}

EXTERN const IID IID_IUnknown_={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

EXTERN const IID IID_ISequentialInStream_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x03,0x00,0x01,0x00,0x00}};
EXTERN const IID IID_ISequentialOutStream_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x03,0x00,0x02,0x00,0x00}};
EXTERN const IID IID_IInStream_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x03,0x00,0x03,0x00,0x00}};
EXTERN const IID IID_IOutStream_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x03,0x00,0x04,0x00,0x00}};

EXTERN const IID IID_ICompressCoder_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x04,0x00,0x05,0x00,0x00}};
EXTERN const IID IID_ICompressSetCoderProperties_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x04,0x00,0x20,0x00,0x00}};
EXTERN const IID IID_ICompressCodecsInfo_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x04,0x00,0x60,0x00,0x00}};

EXTERN const IID IID_ICryptoGetTextPassword_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x05,0x00,0x10,0x00,0x00}};
EXTERN const IID IID_ICryptoGetTextPassword2_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x05,0x00,0x11,0x00,0x00}};

EXTERN const IID IID_IArchiveExtractCallback_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0x20,0x00,0x00}};
EXTERN const IID IID_IArchiveOpenVolumeCallback_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0x30,0x00,0x00}};
EXTERN const IID IID_IInArchive_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0x60,0x00,0x00}};
EXTERN const IID IID_IArchiveUpdateCallback_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0x80,0x00,0x00}};
EXTERN const IID IID_IOutArchive_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0xA0,0x00,0x00}};
EXTERN const IID IID_ISetProperties_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0x03,0x00,0x00}};

static int lzmaGetArchiveFileProperty22(void *archiver,unsigned int index,int kpid,PROPVARIANT *prop);
static int lzmaGetArchiveFileProperty23(void *archiver,unsigned int index,int kpid,PROPVARIANT *prop);
static int _lzmaGuessVersion();

static HRESULT WINAPI SInStreamFile22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SInStreamFile22* self = (SInStreamFile22*)_self;
	if(!memcmp(iid,&IID_IInStream_,sizeof(GUID))){
		*out_obj = self;
		self->vt->AddRef(self);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SInStreamFile23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SInStreamFile23* self = (SInStreamFile23*)_self;
	if(!memcmp(iid,&IID_IInStream_,sizeof(GUID))){
		*out_obj = self;
		self->vt->AddRef(self);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SInStreamFile_AddRef(void* _self){
	LZMA_UNUSED SInStreamFile23* self = (SInStreamFile23*)_self;
	return ++self->refs;
}

static u32 WINAPI SInStreamFile_Release(void* _self){
	LZMA_UNUSED SInStreamFile23* self = (SInStreamFile23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		if(self->f)fclose(self->f);
		self->f=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SInStreamFile_Read(void* _self, void *data, u32 size, u32 *processedSize){
	LZMA_UNUSED SInStreamFile23* self = (SInStreamFile23*)_self;
	u32 readlen = fread(data, 1, size, self->f);
	if(processedSize)*processedSize = readlen;
	return S_OK;
}

static HRESULT WINAPI SInStreamFile_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	LZMA_UNUSED SInStreamFile23* self = (SInStreamFile23*)_self;
	fseeko(self->f, offset, seekOrigin);
	if(newPosition)*newPosition = ftello(self->f);
	return S_OK;
}

bool WINAPI MakeSInStreamFile22(SInStreamFile22 *self, const char *fname){
	self->f = fopen(fname,"rb");
	if(!self->f)return false;
	self->vt = (IInStream22_vt*)calloc(1,sizeof(IInStream22_vt));
	if(!self->vt){
		if(self->f)fclose(self->f);
		self->f=NULL;
		return false;
	}
	self->vt->QueryInterface = SInStreamFile22_QueryInterface;
	self->vt->AddRef = SInStreamFile_AddRef;
	self->vt->Release = SInStreamFile_Release;
	self->vt->Read = SInStreamFile_Read;
	self->vt->Seek = SInStreamFile_Seek;
	self->refs = 1;
	return true;
}

bool WINAPI MakeSInStreamFile23(SInStreamFile23 *self, const char *fname){
	self->f = fopen(fname,"rb");
	if(!self->f)return false;
	self->vt = (IInStream23_vt*)calloc(1,sizeof(IInStream23_vt));
	if(!self->vt){
		if(self->f)fclose(self->f);
		self->f=NULL;
		return false;
	}
	self->vt->QueryInterface = SInStreamFile23_QueryInterface;
	self->vt->AddRef = SInStreamFile_AddRef;
	self->vt->Release = SInStreamFile_Release;
	self->vt->Read = SInStreamFile_Read;
	self->vt->Seek = SInStreamFile_Seek;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SInStreamMem22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SInStreamMem22* self = (SInStreamMem22*)_self;
	if(!memcmp(iid,&IID_IInStream_,sizeof(GUID))){
		*out_obj = self;
		self->vt->AddRef(self);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SInStreamMem23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SInStreamMem23* self = (SInStreamMem23*)_self;
	if(!memcmp(iid,&IID_IInStream_,sizeof(GUID))){
		*out_obj = self;
		self->vt->AddRef(self);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SInStreamMem_AddRef(void* _self){
	LZMA_UNUSED SInStreamMem23* self = (SInStreamMem23*)_self;
	return ++self->refs;
}

static u32 WINAPI SInStreamMem_Release(void* _self){
	LZMA_UNUSED SInStreamMem23* self = (SInStreamMem23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		mclose(self->m);
		self->m=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SInStreamMem_Read(void* _self, void *data, u32 size, u32 *processedSize){
	LZMA_UNUSED SInStreamMem23* self = (SInStreamMem23*)_self;
	u32 readlen = mread(data, size, self->m);
	if(processedSize)*processedSize = readlen;
	return S_OK;
}

static HRESULT WINAPI SInStreamMem_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	LZMA_UNUSED SInStreamMem23* self = (SInStreamMem23*)_self;
	u64 pos = mseek(self->m, offset, seekOrigin);
	if(newPosition)*newPosition = pos;
	return S_OK;
}

bool MakeSInStreamMem22(SInStreamMem22 *self, void *p, const unsigned int size){
	self->m = mopen(p,size,NULL);
	if(!self->m)return false;
	self->vt = (IInStream22_vt*)calloc(1,sizeof(IInStream22_vt));
	if(!self->vt){
		mclose(self->m);
		self->m=NULL;
		return false;
	}
	self->vt->QueryInterface = SInStreamMem22_QueryInterface;
	self->vt->AddRef = SInStreamMem_AddRef;
	self->vt->Release = SInStreamMem_Release;
	self->vt->Read = SInStreamMem_Read;
	self->vt->Seek = SInStreamMem_Seek;
	self->refs = 1;
	return true;
}

bool MakeSInStreamMem23(SInStreamMem23 *self, void *p, const unsigned int size){
	self->m = mopen(p,size,NULL);
	if(!self->m)return false;
	self->vt = (IInStream23_vt*)calloc(1,sizeof(IInStream23_vt));
	if(!self->vt){
		mclose(self->m);
		self->m=NULL;
		return false;
	}
	self->vt->QueryInterface = SInStreamMem23_QueryInterface;
	self->vt->AddRef = SInStreamMem_AddRef;
	self->vt->Release = SInStreamMem_Release;
	self->vt->Read = SInStreamMem_Read;
	self->vt->Seek = SInStreamMem_Seek;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SInStreamGeneric22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SInStreamGeneric22* self = (SInStreamGeneric22*)_self;
	if(!memcmp(iid,&IID_IInStream_,sizeof(GUID))){
		*out_obj = self;
		self->vt->AddRef(self);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SInStreamGeneric23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SInStreamGeneric23* self = (SInStreamGeneric23*)_self;
	if(!memcmp(iid,&IID_IInStream_,sizeof(GUID))){
		*out_obj = self;
		self->vt->AddRef(self);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SInStreamGeneric_AddRef(void* _self){
	LZMA_UNUSED SInStreamGeneric23* self = (SInStreamGeneric23*)_self;
	return ++self->refs;
}

static u32 WINAPI SInStreamGeneric_Release(void* _self){
	LZMA_UNUSED SInStreamGeneric23* self = (SInStreamGeneric23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		if(self->pClose)self->pClose(self->h);
		self->h=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SInStreamGeneric_Read(void* _self, void *data, u32 size, u32 *processedSize){
	LZMA_UNUSED SInStreamGeneric23* self = (SInStreamGeneric23*)_self;
	if(!self->h||!self->pRead)return E_FAIL;
	int readlen=self->pRead(self->h,(char*)data,size);
	if(processedSize)*processedSize=readlen;
	return S_OK;
}

static HRESULT WINAPI SInStreamGeneric_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	LZMA_UNUSED SInStreamGeneric23* self = (SInStreamGeneric23*)_self;
	if(!self->h||!self->pSeek)return E_FAIL;
	if(self->pSeek(self->h,offset,seekOrigin))return E_FAIL;//errno;
	if(newPosition){
		if(!self->pTell)return E_FAIL;
		*newPosition=self->pTell(self->h);
	}
	return S_OK;
}

bool MakeSInStreamGeneric22(SInStreamGeneric22 *self, void *h, tRead pRead, tClose pClose, tSeek pSeek, tTell pTell){
	self->h = h;
	if(!self->h)return false;
	self->pRead = pRead;
	self->pSeek = pSeek;
	self->pClose = pClose;
	self->pTell = pTell;
	self->vt = (IInStream22_vt*)calloc(1,sizeof(IInStream22_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SInStreamGeneric22_QueryInterface;
	self->vt->AddRef = SInStreamGeneric_AddRef;
	self->vt->Release = SInStreamGeneric_Release;
	self->vt->Read = SInStreamGeneric_Read;
	self->vt->Seek = SInStreamGeneric_Seek;
	self->refs = 1;
	return true;
}

bool MakeSInStreamGeneric23(SInStreamGeneric23 *self, void *h, tRead pRead, tClose pClose, tSeek pSeek, tTell pTell){
	self->h = h;
	if(!self->h)return false;
	self->pRead = pRead;
	self->pSeek = pSeek;
	self->pClose = pClose;
	self->pTell = pTell;
	self->vt = (IInStream23_vt*)calloc(1,sizeof(IInStream23_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SInStreamGeneric23_QueryInterface;
	self->vt->AddRef = SInStreamGeneric_AddRef;
	self->vt->Release = SInStreamGeneric_Release;
	self->vt->Read = SInStreamGeneric_Read;
	self->vt->Seek = SInStreamGeneric_Seek;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SOutStreamFile22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SOutStreamFile22* self = (SOutStreamFile22*)_self;
	if(!memcmp(iid,&IID_IOutStream_,sizeof(GUID))){
		*out_obj = self;
		self->vt->AddRef(self);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SOutStreamFile23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SOutStreamFile23* self = (SOutStreamFile23*)_self;
	if(!memcmp(iid,&IID_IOutStream_,sizeof(GUID))){
		*out_obj = self;
		self->vt->AddRef(self);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SOutStreamFile_AddRef(void* _self){
	LZMA_UNUSED SOutStreamFile23* self = (SOutStreamFile23*)_self;
	return ++self->refs;
}

static u32 WINAPI SOutStreamFile_Release(void* _self){
	LZMA_UNUSED SOutStreamFile23* self = (SOutStreamFile23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		if(self->f)fclose(self->f);
		self->f=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SOutStreamFile_Write(void* _self, const void *data, u32 size, u32 *processedSize){
	LZMA_UNUSED SOutStreamFile23* self = (SOutStreamFile23*)_self;
	u32 writelen = fwrite(data, 1, size, self->f);
	if(processedSize)*processedSize = writelen;
	if(size&&!writelen)return E_FAIL;
	return S_OK;
}

static HRESULT WINAPI SOutStreamFile_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	LZMA_UNUSED SOutStreamFile23* self = (SOutStreamFile23*)_self;
	fseeko(self->f, offset, seekOrigin);
	if(newPosition)*newPosition = ftello(self->f);
	return S_OK;
}

static HRESULT WINAPI SOutStreamFile_SetSize(void* _self, u64 newSize){
	LZMA_UNUSED SOutStreamFile23* self = (SOutStreamFile23*)_self;
	ftruncate(fileno(self->f),newSize);
	return S_OK;
}

bool MakeSOutStreamFile22(SOutStreamFile22 *self, const char *fname, bool readable){
	self->f = NULL;
	if(readable){
		self->f = fopen(fname,"r+b");
	}
	if(!self->f){
		self->f = fopen(fname,"wb");
	}
	if(!self->f)return false;
	self->vt = (IOutStream22_vt*)calloc(1,sizeof(IOutStream22_vt));
	if(!self->vt){
		if(self->f)fclose(self->f);
		self->f=NULL;
		return false;
	}
	self->vt->QueryInterface = SOutStreamFile22_QueryInterface;
	self->vt->AddRef = SOutStreamFile_AddRef;
	self->vt->Release = SOutStreamFile_Release;
	self->vt->Write = SOutStreamFile_Write;
	self->vt->Seek = SOutStreamFile_Seek;
	self->vt->SetSize = SOutStreamFile_SetSize;
	self->refs = 1;
	return true;
}

bool MakeSOutStreamFile23(SOutStreamFile23 *self, const char *fname, bool readable){
	self->f = NULL;
	if(readable){
		self->f = fopen(fname,"r+b");
	}
	if(!self->f){
		self->f = fopen(fname,"wb");
	}
	if(!self->f)return false;
	self->vt = (IOutStream23_vt*)calloc(1,sizeof(IOutStream23_vt));
	if(!self->vt){
		if(self->f)fclose(self->f);
		self->f=NULL;
		return false;
	}
	self->vt->QueryInterface = SOutStreamFile23_QueryInterface;
	self->vt->AddRef = SOutStreamFile_AddRef;
	self->vt->Release = SOutStreamFile_Release;
	self->vt->Write = SOutStreamFile_Write;
	self->vt->Seek = SOutStreamFile_Seek;
	self->vt->SetSize = SOutStreamFile_SetSize;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SSequentialOutStreamMem22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SSequentialOutStreamMem22* self = (SSequentialOutStreamMem22*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SSequentialOutStreamMem23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SSequentialOutStreamMem23* self = (SSequentialOutStreamMem23*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SSequentialOutStreamMem_AddRef(void* _self){
	LZMA_UNUSED SSequentialOutStreamMem23* self = (SSequentialOutStreamMem23*)_self;
	return ++self->refs;
}

static u32 WINAPI SSequentialOutStreamMem_Release(void* _self){
	LZMA_UNUSED SSequentialOutStreamMem23* self = (SSequentialOutStreamMem23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		mclose(self->m);
		self->m=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SSequentialOutStreamMem_Write(void* _self, const void *data, u32 size, u32 *processedSize){
	LZMA_UNUSED SSequentialOutStreamMem23* self = (SSequentialOutStreamMem23*)_self;
	u32 writelen = mwrite(data, size, self->m);
	if(processedSize)*processedSize = writelen;
	if(size&&!writelen)return E_FAIL;
	return S_OK;
}

static HRESULT WINAPI SSequentialOutStreamMem_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	LZMA_UNUSED SSequentialOutStreamMem23* self = (SSequentialOutStreamMem23*)_self;
	u64 pos = mseek(self->m, offset, seekOrigin);
	if(newPosition)*newPosition = pos;
	return S_OK;
}

static HRESULT WINAPI SSequentialOutStreamMem_SetSize(void* _self, u64 newSize){
	LZMA_UNUSED SSequentialOutStreamMem23* self = (SSequentialOutStreamMem23*)_self;
	return E_FAIL; // this is why declared as Sequential.
}

bool MakeSSequentialOutStreamMem22(SSequentialOutStreamMem22 *self, void *p, const unsigned int size){
	self->m = mopen(p,size,NULL);
	if(!self->m)return false;
	self->vt = (IOutStream22_vt*)calloc(1,sizeof(IOutStream22_vt));
	if(!self->vt){
		mclose(self->m);
		self->m=NULL;
		return false;
	}
	self->vt->QueryInterface = SSequentialOutStreamMem22_QueryInterface;
	self->vt->AddRef = SSequentialOutStreamMem_AddRef;
	self->vt->Release = SSequentialOutStreamMem_Release;
	self->vt->Write = SSequentialOutStreamMem_Write;
	self->vt->Seek = SSequentialOutStreamMem_Seek;
	self->vt->SetSize = SSequentialOutStreamMem_SetSize;
	self->refs = 1;
	return true;
}

bool MakeSSequentialOutStreamMem23(SSequentialOutStreamMem23 *self, void *p, const unsigned int size){
	self->m = mopen(p,size,NULL);
	if(!self->m)return false;
	self->vt = (IOutStream23_vt*)calloc(1,sizeof(IOutStream23_vt));
	if(!self->vt){
		mclose(self->m);
		self->m=NULL;
		return false;
	}
	self->vt->QueryInterface = SSequentialOutStreamMem23_QueryInterface;
	self->vt->AddRef = SSequentialOutStreamMem_AddRef;
	self->vt->Release = SSequentialOutStreamMem_Release;
	self->vt->Write = SSequentialOutStreamMem_Write;
	self->vt->Seek = SSequentialOutStreamMem_Seek;
	self->vt->SetSize = SSequentialOutStreamMem_SetSize;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SSequentialOutStreamGeneric22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SSequentialOutStreamGeneric22* self = (SSequentialOutStreamGeneric22*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SSequentialOutStreamGeneric23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SSequentialOutStreamGeneric23* self = (SSequentialOutStreamGeneric23*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SSequentialOutStreamGeneric_AddRef(void* _self){
	LZMA_UNUSED SSequentialOutStreamGeneric23* self = (SSequentialOutStreamGeneric23*)_self;
	return ++self->refs;
}

static u32 WINAPI SSequentialOutStreamGeneric_Release(void* _self){
	LZMA_UNUSED SSequentialOutStreamGeneric23* self = (SSequentialOutStreamGeneric23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		if(self->pClose)self->pClose(self->h);
		self->h=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SSequentialOutStreamGeneric_Write(void* _self, const void *data, u32 size, u32 *processedSize){
	LZMA_UNUSED SSequentialOutStreamGeneric23* self = (SSequentialOutStreamGeneric23*)_self;
	if(!self->h||!self->pWrite)return E_FAIL;
	int writelen=self->pWrite(self->h,(char*)data,size);
	if(processedSize)*processedSize=writelen;
	if(size&&!writelen)return E_FAIL;
	return S_OK;
}

static HRESULT WINAPI SSequentialOutStreamGeneric_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	LZMA_UNUSED SSequentialOutStreamGeneric23* self = (SSequentialOutStreamGeneric23*)_self;
	return E_FAIL;
#if 0
	if(!self->h||!self->pSeek)return E_FAIL;
	if(self->pSeek(self->h,offset,seekOrigin))return E_FAIL;//errno;
	if(newPosition){
		if(!self->pTell)return E_FAIL;
		*newPosition=self->pTell(self->h);
	}
	return S_OK;
#endif
}

static HRESULT WINAPI SSequentialOutStreamGeneric_SetSize(void* _self, u64 newSize){
	LZMA_UNUSED SSequentialOutStreamGeneric23* self = (SSequentialOutStreamGeneric23*)_self;
	return E_FAIL;
}

bool MakeSSequentialOutStreamGeneric22(SSequentialOutStreamGeneric22 *self, void *h, tWrite pWrite, tClose pClose){
	self->h = h;
	if(!self->h)return false;
	self->pWrite = pWrite;
	self->pClose = pClose;
	self->vt = (IOutStream22_vt*)calloc(1,sizeof(IOutStream22_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SSequentialOutStreamGeneric22_QueryInterface;
	self->vt->AddRef = SSequentialOutStreamGeneric_AddRef;
	self->vt->Release = SSequentialOutStreamGeneric_Release;
	self->vt->Write = SSequentialOutStreamGeneric_Write;
	self->vt->Seek = SSequentialOutStreamGeneric_Seek;
	self->vt->SetSize = SSequentialOutStreamGeneric_SetSize;
	self->refs = 1;
	return true;
}

bool MakeSSequentialOutStreamGeneric23(SSequentialOutStreamGeneric23 *self, void *h, tWrite pWrite, tClose pClose){
	self->h = h;
	if(!self->h)return false;
	self->pWrite = pWrite;
	self->pClose = pClose;
	self->vt = (IOutStream23_vt*)calloc(1,sizeof(IOutStream23_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SSequentialOutStreamGeneric23_QueryInterface;
	self->vt->AddRef = SSequentialOutStreamGeneric_AddRef;
	self->vt->Release = SSequentialOutStreamGeneric_Release;
	self->vt->Write = SSequentialOutStreamGeneric_Write;
	self->vt->Seek = SSequentialOutStreamGeneric_Seek;
	self->vt->SetSize = SSequentialOutStreamGeneric_SetSize;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SCryptoGetTextPasswordFixed22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SCryptoGetTextPasswordFixed22 *self = (SCryptoGetTextPasswordFixed22*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SCryptoGetTextPasswordFixed23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SCryptoGetTextPasswordFixed23 *self = (SCryptoGetTextPasswordFixed23*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SCryptoGetTextPasswordFixed_AddRef(void* _self){
	LZMA_UNUSED SCryptoGetTextPasswordFixed23 *self = (SCryptoGetTextPasswordFixed23*)_self;
	return ++self->refs;
}

static u32 WINAPI SCryptoGetTextPasswordFixed_Release(void* _self){
	LZMA_UNUSED SCryptoGetTextPasswordFixed23 *self = (SCryptoGetTextPasswordFixed23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		free(self->password);
		self->password=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SCryptoGetTextPasswordFixed_CryptoGetTextPassword(void *_self, BSTR* password){
	LZMA_UNUSED SCryptoGetTextPasswordFixed23 *self = (SCryptoGetTextPasswordFixed23*)_self;
	if(self->password){
		int passwordlen = strlen(self->password);
		*password = SysAllocStringLen(NULL,passwordlen+1);
		u8* passwordStartAddr = ((u8*)*password)-4;
		write32(passwordStartAddr, mbstowcs(*password, self->password, passwordlen+1));
	}
	return S_OK;
}

bool MakeSCryptoGetTextPasswordFixed22(SCryptoGetTextPasswordFixed22 *self, const char *password){
	self->vt = (ICryptoGetTextPassword22_vt*)calloc(1,sizeof(ICryptoGetTextPassword22_vt));
	if(!self->vt)return false;
	self->password = NULL;
	if(password){
		self->password = (char*)malloc(strlen(password)+1);
		strcpy(self->password, password);
	}
	self->vt->QueryInterface = SCryptoGetTextPasswordFixed22_QueryInterface;
	self->vt->AddRef = SCryptoGetTextPasswordFixed_AddRef;
	self->vt->Release = SCryptoGetTextPasswordFixed_Release;
	self->vt->CryptoGetTextPassword = SCryptoGetTextPasswordFixed_CryptoGetTextPassword;
	self->refs = 1;
	return true;
}

bool MakeSCryptoGetTextPasswordFixed23(SCryptoGetTextPasswordFixed23 *self, const char *password){
	self->vt = (ICryptoGetTextPassword23_vt*)calloc(1,sizeof(ICryptoGetTextPassword23_vt));
	if(!self->vt)return false;
	self->password = NULL;
	if(password){
		self->password = (char*)malloc(strlen(password)+1);
		strcpy(self->password, password);
	}
	self->vt->QueryInterface = SCryptoGetTextPasswordFixed23_QueryInterface;
	self->vt->AddRef = SCryptoGetTextPasswordFixed_AddRef;
	self->vt->Release = SCryptoGetTextPasswordFixed_Release;
	self->vt->CryptoGetTextPassword = SCryptoGetTextPasswordFixed_CryptoGetTextPassword;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SCryptoGetTextPassword2Fixed22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SCryptoGetTextPassword2Fixed22 *self = (SCryptoGetTextPassword2Fixed22*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SCryptoGetTextPassword2Fixed23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SCryptoGetTextPassword2Fixed23 *self = (SCryptoGetTextPassword2Fixed23*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SCryptoGetTextPassword2Fixed_AddRef(void* _self){
	LZMA_UNUSED SCryptoGetTextPassword2Fixed23 *self = (SCryptoGetTextPassword2Fixed23*)_self;
	return ++self->refs;
}

static u32 WINAPI SCryptoGetTextPassword2Fixed_Release(void* _self){
	LZMA_UNUSED SCryptoGetTextPassword2Fixed23 *self = (SCryptoGetTextPassword2Fixed23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		free(self->password);
		self->password=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SCryptoGetTextPassword2Fixed_CryptoGetTextPassword2(void *_self, s32 *passwordIsDefined, BSTR* password){
	LZMA_UNUSED SCryptoGetTextPassword2Fixed23 *self = (SCryptoGetTextPassword2Fixed23*)_self;
	*passwordIsDefined = 0;
	if(self->password){
		*passwordIsDefined = 1;
		int passwordlen = strlen(self->password);
		*password = SysAllocStringLen(NULL,passwordlen+1);
		u8* passwordStartAddr = ((u8*)*password)-4;
		write32(passwordStartAddr, mbstowcs(*password, self->password, passwordlen+1));
	}
	return S_OK;
}

bool MakeSCryptoGetTextPassword2Fixed22(SCryptoGetTextPassword2Fixed22 *self, const char *password){
	self->vt = (ICryptoGetTextPassword222_vt*)calloc(1,sizeof(ICryptoGetTextPassword222_vt));
	if(!self->vt)return false;
	self->password = NULL;
	if(password){
		self->password = (char*)malloc(strlen(password)+1);
		strcpy(self->password, password);
	}
	self->vt->QueryInterface = SCryptoGetTextPassword2Fixed22_QueryInterface;
	self->vt->AddRef = SCryptoGetTextPassword2Fixed_AddRef;
	self->vt->Release = SCryptoGetTextPassword2Fixed_Release;
	self->vt->CryptoGetTextPassword2 = SCryptoGetTextPassword2Fixed_CryptoGetTextPassword2;
	self->refs = 1;
	return true;
}

bool MakeSCryptoGetTextPassword2Fixed23(SCryptoGetTextPassword2Fixed23 *self, const char *password){
	self->vt = (ICryptoGetTextPassword223_vt*)calloc(1,sizeof(ICryptoGetTextPassword223_vt));
	if(!self->vt)return false;
	self->password = NULL;
	if(password){
		self->password = (char*)malloc(strlen(password)+1);
		strcpy(self->password, password);
	}
	self->vt->QueryInterface = SCryptoGetTextPassword2Fixed23_QueryInterface;
	self->vt->AddRef = SCryptoGetTextPassword2Fixed_AddRef;
	self->vt->Release = SCryptoGetTextPassword2Fixed_Release;
	self->vt->CryptoGetTextPassword2 = SCryptoGetTextPassword2Fixed_CryptoGetTextPassword2;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SArchiveOpenVolumeCallback22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SArchiveOpenVolumeCallback22 *self = (SArchiveOpenVolumeCallback22*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SArchiveOpenVolumeCallback23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SArchiveOpenVolumeCallback23 *self = (SArchiveOpenVolumeCallback23*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SArchiveOpenVolumeCallback_AddRef(void* _self){
	LZMA_UNUSED SArchiveOpenVolumeCallback23 *self = (SArchiveOpenVolumeCallback23*)_self;
	return ++self->refs;
}

static u32 WINAPI SArchiveOpenVolumeCallback_Release(void* _self){
	LZMA_UNUSED SArchiveOpenVolumeCallback23 *self = (SArchiveOpenVolumeCallback23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		free(self->fname);
		self->fname=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SArchiveOpenVolumeCallback_GetProperty(void* _self, PROPID propID, PROPVARIANT *value){
	LZMA_UNUSED SArchiveOpenVolumeCallback23 *self = (SArchiveOpenVolumeCallback23*)_self;
	if(propID == kpidName || propID==kpidPath){
		if(self->fname){
			value->vt = VT_BSTR;
			int fnamelen = strlen(self->fname);
			value->bstrVal = SysAllocStringLen(NULL,fnamelen+1);
			u8* startAddr = ((u8*)value->bstrVal)-4;
			write32(startAddr, mbstowcs(value->bstrVal, self->fname, fnamelen+1));
		}
	}
	return S_OK;
}

static HRESULT WINAPI SArchiveOpenVolumeCallback22_GetStream(void* _self, const wchar_t *name, IInStream22_ **inStream){
	LZMA_UNUSED SArchiveOpenVolumeCallback22 *self = (SArchiveOpenVolumeCallback22*)_self;
	IInStream22_* stream = (IInStream22_*)calloc(1,sizeof(SInStreamFile22));
	size_t mbnamelen = wcslen(name)*4;
	char *mbname = (char*)malloc(mbnamelen);
	if(!mbname)return E_FAIL;
	wcstombs(mbname,name,mbnamelen);
	if(!MakeSInStreamFile22((SInStreamFile22*)stream,mbname)){
		free(mbname);
		stream->vt->Release(&stream);
		return E_FAIL;
	}
	*inStream = stream; // WILL BE RELEASED AUTOMATICALLY.
	free(mbname);
	return S_OK;
}

static HRESULT WINAPI SArchiveOpenVolumeCallback23_GetStream(void* _self, const wchar_t *name, IInStream23_ **inStream){
	LZMA_UNUSED SArchiveOpenVolumeCallback23 *self = (SArchiveOpenVolumeCallback23*)_self;
	IInStream23_* stream = (IInStream23_*)calloc(1,sizeof(SInStreamFile23));
	size_t mbnamelen = wcslen(name)*4;
	char *mbname = (char*)malloc(mbnamelen);
	if(!mbname)return E_FAIL;
	wcstombs(mbname,name,mbnamelen);
	if(!MakeSInStreamFile23((SInStreamFile23*)stream,mbname)){
		free(mbname);
		stream->vt->Release(&stream);
		return E_FAIL;
	}
	*inStream = stream; // WILL BE RELEASED AUTOMATICALLY.
	free(mbname);
	return S_OK;
}

bool MakeSArchiveOpenVolumeCallback22(SArchiveOpenVolumeCallback22 *self, const char *fname){
	self->vt = (IArchiveOpenVolumeCallback22_vt*)calloc(1,sizeof(IArchiveOpenVolumeCallback22_vt));
	if(!self->vt)return false;
	self->fname = NULL;
	if(fname){
		self->fname = (char*)malloc(strlen(fname)+1);
		strcpy(self->fname, fname);
	}
	self->vt->QueryInterface = SArchiveOpenVolumeCallback22_QueryInterface;
	self->vt->AddRef = SArchiveOpenVolumeCallback_AddRef;
	self->vt->Release = SArchiveOpenVolumeCallback_Release;
	self->vt->GetProperty = SArchiveOpenVolumeCallback_GetProperty;
	self->vt->GetStream = SArchiveOpenVolumeCallback22_GetStream;
	self->refs = 1;
	return true;
}

bool MakeSArchiveOpenVolumeCallback23(SArchiveOpenVolumeCallback23 *self, const char *fname){
	self->vt = (IArchiveOpenVolumeCallback23_vt*)calloc(1,sizeof(IArchiveOpenVolumeCallback23_vt));
	if(!self->vt)return false;
	self->fname = NULL;
	if(fname){
		self->fname = (char*)malloc(strlen(fname)+1);
		strcpy(self->fname, fname);
	}
	self->vt->QueryInterface = SArchiveOpenVolumeCallback23_QueryInterface;
	self->vt->AddRef = SArchiveOpenVolumeCallback_AddRef;
	self->vt->Release = SArchiveOpenVolumeCallback_Release;
	self->vt->GetProperty = SArchiveOpenVolumeCallback_GetProperty;
	self->vt->GetStream = SArchiveOpenVolumeCallback23_GetStream;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SArchiveOpenCallbackPassword22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SArchiveOpenCallbackPassword22 *self = (SArchiveOpenCallbackPassword22*)_self;
	if(!memcmp(iid,&IID_ICryptoGetTextPassword_,sizeof(GUID))){
		*out_obj = &self->setpassword;
		self->setpassword.vt->AddRef(&self->setpassword);
		return S_OK;
	}
	if(!memcmp(iid,&IID_IArchiveOpenVolumeCallback_,sizeof(GUID))){
		*out_obj = &self->openvolume;
		self->openvolume.vt->AddRef(&self->openvolume);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SArchiveOpenCallbackPassword23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SArchiveOpenCallbackPassword23 *self = (SArchiveOpenCallbackPassword23*)_self;
	if(!memcmp(iid,&IID_ICryptoGetTextPassword_,sizeof(GUID))){
		*out_obj = &self->setpassword;
		self->setpassword.vt->AddRef(&self->setpassword);
		return S_OK;
	}
	if(!memcmp(iid,&IID_IArchiveOpenVolumeCallback_,sizeof(GUID))){
		*out_obj = &self->openvolume;
		self->openvolume.vt->AddRef(&self->openvolume);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SArchiveOpenCallbackPassword_AddRef(void* _self){
	LZMA_UNUSED SArchiveOpenCallbackPassword23 *self = (SArchiveOpenCallbackPassword23*)_self;
	return ++self->refs;
}

static u32 WINAPI SArchiveOpenCallbackPassword22_Release(void* _self){
	LZMA_UNUSED SArchiveOpenCallbackPassword22 *self = (SArchiveOpenCallbackPassword22*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		self->setpassword.vt->Release(&self->setpassword);
		self->openvolume.vt->Release(&self->openvolume);
	}
	return self->refs;
}

static u32 WINAPI SArchiveOpenCallbackPassword23_Release(void* _self){
	LZMA_UNUSED SArchiveOpenCallbackPassword23 *self = (SArchiveOpenCallbackPassword23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		self->setpassword.vt->Release(&self->setpassword);
		self->openvolume.vt->Release(&self->openvolume);
	}
	return self->refs;
}

static HRESULT WINAPI SArchiveOpenCallbackPassword_SetTotal(void* _self, const u64 *files, const u64 *bytes){
	LZMA_UNUSED SArchiveOpenCallbackPassword23 *self = (SArchiveOpenCallbackPassword23*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveOpenCallbackPassword_SetCompleted(void* _self, const u64 *files, const u64 *bytes){
	LZMA_UNUSED SArchiveOpenCallbackPassword23 *self = (SArchiveOpenCallbackPassword23*)_self;
	return S_OK;
}

bool MakeSArchiveOpenCallbackPassword22(SArchiveOpenCallbackPassword22 *self, const char *password, const char *fname){
	self->vt = (IArchiveOpenCallback22_vt*)calloc(1,sizeof(IArchiveOpenCallback22_vt));
	if(!self->vt)return false;
	MakeSCryptoGetTextPasswordFixed22(&self->setpassword,password);
	MakeSArchiveOpenVolumeCallback22(&self->openvolume,fname);
	self->vt->QueryInterface = SArchiveOpenCallbackPassword22_QueryInterface;
	self->vt->AddRef = SArchiveOpenCallbackPassword_AddRef;
	self->vt->Release = SArchiveOpenCallbackPassword22_Release;
	self->vt->SetTotal = SArchiveOpenCallbackPassword_SetTotal;
	self->vt->SetCompleted = SArchiveOpenCallbackPassword_SetCompleted;
	self->refs = 1;
	return true;
}

bool MakeSArchiveOpenCallbackPassword23(SArchiveOpenCallbackPassword23 *self, const char *password, const char *fname){
	self->vt = (IArchiveOpenCallback23_vt*)calloc(1,sizeof(IArchiveOpenCallback23_vt));
	if(!self->vt)return false;
	MakeSCryptoGetTextPasswordFixed23(&self->setpassword,password);
	MakeSArchiveOpenVolumeCallback23(&self->openvolume,fname);
	self->vt->QueryInterface = SArchiveOpenCallbackPassword23_QueryInterface;
	self->vt->AddRef = SArchiveOpenCallbackPassword_AddRef;
	self->vt->Release = SArchiveOpenCallbackPassword23_Release;
	self->vt->SetTotal = SArchiveOpenCallbackPassword_SetTotal;
	self->vt->SetCompleted = SArchiveOpenCallbackPassword_SetCompleted;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SArchiveExtractCallbackBare22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SArchiveExtractCallbackBare22 *self = (SArchiveExtractCallbackBare22*)_self;
	if(!memcmp(iid,&IID_ICryptoGetTextPassword_,sizeof(GUID))){
		*out_obj = &self->setpassword;
		self->setpassword.vt->AddRef(&self->setpassword);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SArchiveExtractCallbackBare23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	if(!memcmp(iid,&IID_ICryptoGetTextPassword_,sizeof(GUID))){
		*out_obj = &self->setpassword;
		self->setpassword.vt->AddRef(&self->setpassword);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SArchiveExtractCallbackBare_AddRef(void* _self){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	return ++self->refs;
}

static u32 WINAPI SArchiveExtractCallbackBare_Release(void* _self){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_SetTotal(void* _self, const u64 total){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_SetCompleted(void* _self, const u64 *completedValue){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveExtractCallbackBare22_GetStream(void* _self, u32 index, /*ISequentialOutStream22_*/IOutStream22_ **outStream, s32 askExtractMode){
	LZMA_UNUSED SArchiveExtractCallbackBare22 *self = (SArchiveExtractCallbackBare22*)_self;
	*outStream = NULL;
	PROPVARIANT path;
	memset(&path,0,sizeof(PROPVARIANT));
	self->lastIndex = index;
	lzmaGetArchiveFileProperty22(self->archiver, index, kpidPath, &path);
	//printf("%d\t%ls\n",index,path.bstrVal);
	IOutStream22_* stream = (IOutStream22_*)calloc(1,sizeof(SOutStreamFile22));
	MakeSOutStreamFile22((SOutStreamFile22*)stream,"/dev/null",false);
	*outStream = stream; // WILL BE RELEASED AUTOMATICALLY.
	PropVariantClear(&path);
	return S_OK;
}

static HRESULT WINAPI SArchiveExtractCallbackBare23_GetStream(void* _self, u32 index, /*ISequentialOutStream23_*/IOutStream23_ **outStream, s32 askExtractMode){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	*outStream = NULL;
	PROPVARIANT path;
	memset(&path,0,sizeof(PROPVARIANT));
	self->lastIndex = index;
	lzmaGetArchiveFileProperty23(self->archiver, index, kpidPath, &path);
	//printf("%d\t%ls\n",index,path.bstrVal);
	IOutStream23_* stream = (IOutStream23_*)calloc(1,sizeof(SOutStreamFile23));
	MakeSOutStreamFile23((SOutStreamFile23*)stream,"/dev/null",false);
	*outStream = stream; // WILL BE RELEASED AUTOMATICALLY.
	PropVariantClear(&path);
	return S_OK;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_PrepareOperation(void* _self, s32 askExtractMode){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_SetOperationResult(void* _self, s32 opRes){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	return S_OK;
}

bool MakeSArchiveExtractCallbackBare22(SArchiveExtractCallbackBare22 *self, IInArchive22_ *archiver, const char *password){
	self->vt = (IArchiveExtractCallback22_vt*)calloc(1,sizeof(IArchiveExtractCallback22_vt));
	if(!self->vt)return false;
	MakeSCryptoGetTextPasswordFixed22(&self->setpassword,password);
	self->archiver = archiver;
	self->lastIndex = -1;
	self->vt->QueryInterface = SArchiveExtractCallbackBare22_QueryInterface;
	self->vt->AddRef = SArchiveExtractCallbackBare_AddRef;
	self->vt->Release = SArchiveExtractCallbackBare_Release;
	self->vt->SetTotal = SArchiveExtractCallbackBare_SetTotal;
	self->vt->SetCompleted = SArchiveExtractCallbackBare_SetCompleted;
	self->vt->GetStream = SArchiveExtractCallbackBare22_GetStream;
	self->vt->PrepareOperation = SArchiveExtractCallbackBare_PrepareOperation;
	self->vt->SetOperationResult = SArchiveExtractCallbackBare_SetOperationResult;
	self->refs = 1;
	return true;
}

bool MakeSArchiveExtractCallbackBare23(SArchiveExtractCallbackBare23 *self, IInArchive23_ *archiver, const char *password){
	self->vt = (IArchiveExtractCallback23_vt*)calloc(1,sizeof(IArchiveExtractCallback23_vt));
	if(!self->vt)return false;
	MakeSCryptoGetTextPasswordFixed23(&self->setpassword,password);
	self->archiver = archiver;
	self->lastIndex = -1;
	self->vt->QueryInterface = SArchiveExtractCallbackBare23_QueryInterface;
	self->vt->AddRef = SArchiveExtractCallbackBare_AddRef;
	self->vt->Release = SArchiveExtractCallbackBare_Release;
	self->vt->SetTotal = SArchiveExtractCallbackBare_SetTotal;
	self->vt->SetCompleted = SArchiveExtractCallbackBare_SetCompleted;
	self->vt->GetStream = SArchiveExtractCallbackBare23_GetStream;
	self->vt->PrepareOperation = SArchiveExtractCallbackBare_PrepareOperation;
	self->vt->SetOperationResult = SArchiveExtractCallbackBare_SetOperationResult;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SArchiveUpdateCallbackBare22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SArchiveUpdateCallbackBare22 *self = (SArchiveUpdateCallbackBare22*)_self;
	if(!memcmp(iid,&IID_ICryptoGetTextPassword2_,sizeof(GUID))){
		*out_obj = &self->setpassword;
		self->setpassword.vt->AddRef(&self->setpassword);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT WINAPI SArchiveUpdateCallbackBare23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SArchiveUpdateCallbackBare23 *self = (SArchiveUpdateCallbackBare23*)_self;
	if(!memcmp(iid,&IID_ICryptoGetTextPassword2_,sizeof(GUID))){
		*out_obj = &self->setpassword;
		self->setpassword.vt->AddRef(&self->setpassword);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SArchiveUpdateCallbackBare_AddRef(void* _self){
	LZMA_UNUSED SArchiveUpdateCallbackBare23 *self = (SArchiveUpdateCallbackBare23*)_self;
	return ++self->refs;
}

static u32 WINAPI SArchiveUpdateCallbackBare_Release(void* _self){
	LZMA_UNUSED SArchiveUpdateCallbackBare23 *self = (SArchiveUpdateCallbackBare23*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SArchiveUpdateCallbackBare_SetTotal(void* _self, const u64 total){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveUpdateCallbackBare_SetCompleted(void* _self, const u64 *completedValue){
	LZMA_UNUSED SArchiveUpdateCallbackBare23 *self = (SArchiveUpdateCallbackBare23*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveUpdateCallbackBare_GetUpdateItemInfo(void* _self, u32 index, s32 *newData, s32 *newProps, u32 *indexInArchive){
	LZMA_UNUSED SArchiveUpdateCallbackBare23 *self = (SArchiveUpdateCallbackBare23*)_self;
	*newData = 1;
	*newProps = 1;
	*indexInArchive = -1;
	return S_OK;
}

static HRESULT WINAPI SArchiveUpdateCallbackBare_GetProperty(void* _self, u32 index, PROPID propID, PROPVARIANT *value){
	LZMA_UNUSED SArchiveUpdateCallbackBare23 *self = (SArchiveUpdateCallbackBare23*)_self;
	//printf("%d %d\n",index,propID);
	if(propID == kpidPath){
		value->vt = VT_BSTR;
		value->bstrVal = SysAllocStringLen(NULL,1);
		wcscpy(value->bstrVal,L"$");
	}else if(propID == kpidIsDir || propID == kpidIsAnti){
		value->vt = VT_BOOL;
		value->boolVal = VARIANT_FALSE;
	}else if(propID == kpidSize){
		value->vt = VT_UI8;
		value->ulVal = 1; ///
	}else if(propID == kpidMTime){
		value->vt = VT_FILETIME;
	}else if(propID == kpidAttrib){
		value->vt = VT_UI4;
	}
	return S_OK;
}

static HRESULT WINAPI SArchiveUpdateCallbackBare22_GetStream(void* _self, u32 index, /*ISequentialInStream22_*/IInStream22_ **inStream){
	LZMA_UNUSED SArchiveExtractCallbackBare22 *self = (SArchiveExtractCallbackBare22*)_self;
	*inStream = NULL;
	//PROPVARIANT path;
	//memset(&path,0,sizeof(PROPVARIANT));
	self->lastIndex = index;
	IInStream22_* stream = (IInStream22_*)calloc(1,sizeof(SInStreamFile22));
	MakeSInStreamFile22((SInStreamFile22*)stream,"/dev/null");
	*inStream = stream; // WILL BE RELEASED AUTOMATICALLY.
	//PropVariantClear(&path);
	return S_OK;
}

static HRESULT WINAPI SArchiveUpdateCallbackBare23_GetStream(void* _self, u32 index, /*ISequentialInStream23_*/IInStream23_ **inStream){
	LZMA_UNUSED SArchiveExtractCallbackBare23 *self = (SArchiveExtractCallbackBare23*)_self;
	*inStream = NULL;
	//PROPVARIANT path;
	//memset(&path,0,sizeof(PROPVARIANT));
	self->lastIndex = index;
	IInStream23_* stream = (IInStream23_*)calloc(1,sizeof(SInStreamFile23));
	MakeSInStreamFile23((SInStreamFile23*)stream,"/dev/null");
	*inStream = stream; // WILL BE RELEASED AUTOMATICALLY.
	//PropVariantClear(&path);
	return S_OK;
}

static HRESULT WINAPI SArchiveUpdateCallbackBare_SetOperationResult(void* _self, s32 opRes){
	LZMA_UNUSED SArchiveUpdateCallbackBare23 *self = (SArchiveUpdateCallbackBare23*)_self;
	return S_OK;
}

bool MakeSArchiveUpdateCallbackBare22(SArchiveUpdateCallbackBare22 *self, IOutArchive22_ *archiver, const char *password){
	self->vt = (IArchiveUpdateCallback22_vt*)calloc(1,sizeof(IArchiveUpdateCallback22_vt));
	if(!self->vt)return false;
	MakeSCryptoGetTextPassword2Fixed22(&self->setpassword,password);
	self->archiver = archiver;
	self->lastIndex = -1;
	self->vt->QueryInterface = SArchiveUpdateCallbackBare22_QueryInterface;
	self->vt->AddRef = SArchiveUpdateCallbackBare_AddRef;
	self->vt->Release = SArchiveUpdateCallbackBare_Release;
	self->vt->SetTotal = SArchiveUpdateCallbackBare_SetTotal;
	self->vt->SetCompleted = SArchiveUpdateCallbackBare_SetCompleted;
	self->vt->GetUpdateItemInfo = SArchiveUpdateCallbackBare_GetUpdateItemInfo;
	self->vt->GetProperty = SArchiveUpdateCallbackBare_GetProperty;
	self->vt->GetStream = SArchiveUpdateCallbackBare22_GetStream;
	self->vt->SetOperationResult = SArchiveUpdateCallbackBare_SetOperationResult;
	self->refs = 1;
	return true;
}

bool MakeSArchiveUpdateCallbackBare23(SArchiveUpdateCallbackBare23 *self, IOutArchive23_ *archiver, const char *password){
	self->vt = (IArchiveUpdateCallback23_vt*)calloc(1,sizeof(IArchiveUpdateCallback22_vt));
	if(!self->vt)return false;
	MakeSCryptoGetTextPassword2Fixed23(&self->setpassword,password);
	self->archiver = archiver;
	self->lastIndex = -1;
	self->vt->QueryInterface = SArchiveUpdateCallbackBare23_QueryInterface;
	self->vt->AddRef = SArchiveUpdateCallbackBare_AddRef;
	self->vt->Release = SArchiveUpdateCallbackBare_Release;
	self->vt->SetTotal = SArchiveUpdateCallbackBare_SetTotal;
	self->vt->SetCompleted = SArchiveUpdateCallbackBare_SetCompleted;
	self->vt->GetUpdateItemInfo = SArchiveUpdateCallbackBare_GetUpdateItemInfo;
	self->vt->GetProperty = SArchiveUpdateCallbackBare_GetProperty;
	self->vt->GetStream = SArchiveUpdateCallbackBare23_GetStream;
	self->vt->SetOperationResult = SArchiveUpdateCallbackBare_SetOperationResult;
	self->refs = 1;
	return true;
}

//Loading 7z.so
typedef HRESULT (WINAPI*funcCreateObject)(const GUID*,const GUID*,void**);
static funcCreateObject pCreateArchiver,pCreateCoder;
typedef HRESULT (WINAPI*funcSetCodecs22)(ICompressCodecsInfo22_ *compressCodecsInfo);
static funcSetCodecs22 pSetCodecs22;
typedef HRESULT (WINAPI*funcSetCodecs23)(ICompressCodecsInfo23_ *compressCodecsInfo);
static funcSetCodecs23 pSetCodecs23;
typedef HRESULT (WINAPI*funcGetHashers22)(IHashers22_ **hashers);
static funcGetHashers22 pGetHashers22;
typedef HRESULT (WINAPI*funcGetHashers23)(IHashers23_ **hashers);
static funcGetHashers23 pGetHashers23;
typedef HRESULT (WINAPI*funcGetNumMethods)(u32 *numMethods);
static funcGetNumMethods pGetNumMethods;
typedef HRESULT (WINAPI*funcGetNumFormats)(u32 *numFormats);
static funcGetNumFormats pGetNumFormats;
typedef HRESULT (WINAPI*funcGetProperty)(u32 index, PROPID propID, PROPVARIANT *value);
static funcGetProperty pGetMethodProperty;
static funcGetProperty pGetHandlerProperty2;
typedef HRESULT (WINAPI*funcCreateEncoder)(u32 index, const GUID *iid, void **coder);
typedef HRESULT (WINAPI*funcCreateDecoder)(u32 index, const GUID *iid, void **coder);

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
	typedef void* HMODULE;
	#include <pwd.h>
	#include <unistd.h>
#endif

static HMODULE h7z=NULL;
static int guessed7zVersion = 0;

typedef struct{
	ICompressCodecsInfo22_vt *vt;
	u32 refs;

	struct scv_vector *vCodecs;
	struct scv_vector *vNumMethods;
	struct scv_vector *vGetMethodProperty;
	struct scv_vector *vCreateDecoder;
	struct scv_vector *vCreateEncoder;
} SCompressCodecsInfoExternal22_;
static SCompressCodecsInfoExternal22_ scoder22 = {NULL, 0, NULL, NULL, NULL, NULL, NULL};

typedef struct{
	ICompressCodecsInfo23_vt *vt;
	u32 refs;

	struct scv_vector *vCodecs;
	struct scv_vector *vNumMethods;
	struct scv_vector *vGetMethodProperty;
	struct scv_vector *vCreateDecoder;
	struct scv_vector *vCreateEncoder;
} SCompressCodecsInfoExternal23_;
static SCompressCodecsInfoExternal23_ scoder23 = {NULL, 0, NULL, NULL, NULL, NULL, NULL};

static HRESULT WINAPI SCompressCodecsInfoExternal_GetNumMethods(void* _self, u32 *numMethods);
static HRESULT WINAPI SCompressCodecsInfoExternal_GetProperty(void* _self, u32 index, PROPID propID, PROPVARIANT *value);

int lzmaOpen7z(){
	if(lzma7zAlive())return 0;

	h7z=NULL;
#ifndef NODLOPEN
	//getenv is useful but it can cause security matter
	//if(!h7z && getenv("MINI7Z_IMPL"))h7z=LoadLibraryA(getenv("MINI7Z_IMPL"));
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	if(!h7z)h7z=LoadLibraryA("C:\\Program Files\\7-Zip\\7z.dll");
	if(!h7z)h7z=LoadLibraryA("C:\\Program Files (x86)\\7-Zip\\7z.dll");
	if(!h7z)h7z=LoadLibraryA("7z.dll"); // last resort using PATH
	if(!h7z)h7z=LoadLibraryA("7z64.dll"); // lol... (for example, could be useful for Wine testing)
	if(!h7z)h7z=LoadLibraryA("7z32.dll"); // lol...
#else
	// 7-zip-full
	if(!h7z)h7z=LoadLibraryA("/usr/local/opt/7-zip-full/lib/7-zip-full/7z.so"); //Homebrew
	if(!h7z)h7z=LoadLibraryA("/opt/homebrew/opt/7-zip-full/lib/7-zip-full/7z.so");
	if(!h7z)h7z=LoadLibraryA("/home/linuxbrew/.linuxbrew/lib/7-zip-full/7z.so"); // https://github.com/cielavenir/homebrew-ciel/blob/master/sevenzip.rb
	// if(!h7z)h7z=LoadLibraryA("/opt/local/lib/7-zip-full/7z.so"); //MacPorts
	// if(!h7z)h7z=LoadLibraryA("/sw/lib/7-zip-full/7z.so"); //Fink
	if(!h7z){
#if !defined(FEOS) && !defined(__ANDROID__)
		size_t buflen = sysconf(_SC_GETPW_R_SIZE_MAX)+128;
		char *buf = malloc(buflen);
		struct passwd pwd, *r=NULL;
		getpwuid_r(getuid(), &pwd, buf, buflen, &r);
		char *home = pwd.pw_dir;
		//char *home = getenv("HOME");
		if(home){
			char buf[256];
			sprintf(buf,"%s/.linuxbrew/lib/7-zip-full/7z.so",home);
			h7z=LoadLibraryA(buf);
		}
		free(buf);
#endif
	}
	if(!h7z)h7z=LoadLibraryA("/usr/lib/7-zip-full/7z.so"); //Generic
	if(!h7z)h7z=LoadLibraryA("/usr/local/lib/7-zip-full/7z.so");
	if(!h7z)h7z=LoadLibraryA("/usr/libexec/7-zip-full/7z.so");
	// 7zip (Debian customize)
	if(!h7z)h7z=LoadLibraryA("/usr/lib/7zip/7z.so"); //Generic
	if(!h7z)h7z=LoadLibraryA("/usr/local/lib/7zip/7z.so");
	if(!h7z)h7z=LoadLibraryA("/usr/libexec/7zip/7z.so");
	// p7zip
	if(!h7z)h7z=LoadLibraryA("/usr/local/opt/p7zip/lib/p7zip/7z.so"); //Homebrew
	if(!h7z)h7z=LoadLibraryA("/opt/homebrew/opt/p7zip/lib/p7zip/7z.so");
	if(!h7z)h7z=LoadLibraryA("/opt/local/lib/p7zip/7z.so"); //MacPorts
	if(!h7z)h7z=LoadLibraryA("/sw/lib/p7zip/7z.so"); //Fink
	if(!h7z)h7z=LoadLibraryA("/home/linuxbrew/.linuxbrew/lib/p7zip/7z.so");
	if(!h7z){
#if !defined(FEOS) && !defined(__ANDROID__)
		size_t buflen = sysconf(_SC_GETPW_R_SIZE_MAX)+128;
		char *buf = malloc(buflen);
		struct passwd pwd, *r=NULL;
		getpwuid_r(getuid(), &pwd, buf, buflen, &r);
		char *home = pwd.pw_dir;
		//char *home = getenv("HOME");
		if(home){
			char buf[256];
			sprintf(buf,"%s/.linuxbrew/lib/p7zip/7z.so",home);
			h7z=LoadLibraryA(buf);
		}
		free(buf);
#endif
	}
	if(!h7z)h7z=LoadLibraryA("/usr/lib/p7zip/7z.so"); //Generic
	if(!h7z)h7z=LoadLibraryA("/usr/local/lib/p7zip/7z.so");
	if(!h7z)h7z=LoadLibraryA("/usr/libexec/p7zip/7z.so");
	if(!h7z)h7z=LoadLibraryA("7z.so"); // last resort using LD_LIBRARY_PATH
	if(!h7z)h7z=LoadLibraryA("7z.dylib");
#endif
#endif
	if(!h7z)return 1;

#ifndef NODLOPEN
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	pCreateArchiver=(funcCreateObject)GetProcAddress(h7z,"CreateObject");
	pCreateCoder=(funcCreateObject)GetProcAddress(h7z,"CreateObject");
#else
	pCreateArchiver=(funcCreateObject)GetProcAddress(h7z,"CreateArchiver");
	pCreateCoder=(funcCreateObject)GetProcAddress(h7z,"CreateCoder");
#endif
	pSetCodecs22=(funcSetCodecs22)GetProcAddress(h7z,"SetCodecs");
	pSetCodecs23=(funcSetCodecs23)GetProcAddress(h7z,"SetCodecs");
	pGetHashers22=(funcGetHashers22)GetProcAddress(h7z,"GetHashers");
	pGetHashers23=(funcGetHashers23)GetProcAddress(h7z,"GetHashers");
	pGetNumFormats=(funcGetNumFormats)GetProcAddress(h7z,"GetNumberOfFormats");
	pGetNumMethods=(funcGetNumMethods)GetProcAddress(h7z,"GetNumberOfMethods");
	pGetHandlerProperty2 = (funcGetProperty)GetProcAddress(h7z,"GetHandlerProperty2");
	pGetMethodProperty = (funcGetProperty)GetProcAddress(h7z,"GetMethodProperty");
#endif
	if(!lzma7zAlive()){
#ifndef NODLOPEN
		FreeLibrary(h7z);
#endif
		h7z=NULL;
		return 2;
	}

	guessed7zVersion = _lzmaGuessVersion();

	return 0; //now you can call 7z.so.
}
bool lzma7zAlive(){
	return h7z&&pCreateArchiver&&pCreateCoder&&pSetCodecs22&&pSetCodecs23&&pGetHashers22&&pGetHashers23&&pGetNumFormats&&pGetNumMethods&&pGetHandlerProperty2&&pGetMethodProperty;
}
int lzmaGet7zFileName(char* path, int siz){
	if(!lzma7zAlive())return 0;
#if 0
//defined(DL_ANDROID)
	return GetModuleFileNameA(pCreateArchiver,path,siz);
#else
	return GetModuleFileNameA(h7z,path,siz);
#endif
}
int lzmaClose7z(){
	if(!lzma7zAlive())return 1;
#ifndef NODLOPEN
	FreeLibrary(h7z);
#endif
	h7z=NULL;
	return 0;
}

static int _lzmaGuessVersion(){
	if(!h7z)return -1;
	int ret = 0;

	// hasher 0x00000231 (SHA3-256) 2409

	u32 numMethods;
	pGetNumMethods(&numMethods);
	for(u32 i=0;i<numMethods;i++){
		PROPVARIANT value;
		pGetMethodProperty(i, NMethodPropID_kID, &value);
		if(value.uintVal == 0x00040109)ret=max32(ret, 230); // deflate64 (2.30 Beta 26)
		if(value.uintVal == 0x00000021)ret=max32(ret, 904); // lzma2
		if(value.uintVal == 0x0000000a)ret=max32(ret,2300); // arm64
		if(value.uintVal == 0x0000000b)ret=max32(ret,2403); // riscv
	}
	u32 numFormats;
	pGetNumFormats(&numFormats);
	for(u32 i=0;i<numFormats;i++){
		PROPVARIANT value;
		pGetHandlerProperty2(i, NHandlerPropID_kClassID, &value);
		u8 id = ((u8*)value.bstrVal)[13];
		if(id == 0x06)ret=max32(ret, 426); // lzh
		if(id == 0x09)ret=max32(ret, 442); // nsis
		if(id == 0x0c)ret=max32(ret, 904); // xz
		if(id == 0x0e && ret >= 2300)ret=max32(ret,2401); // zstd // recognize zstd as flag only when arm64 is set
		if(id == 0xbf)ret=max32(ret,2405); // lvm
		if(id == 0xc3)ret=max32(ret,2200); // apfs
		if(id == 0xc4)ret=max32(ret,2107); // vhdx
		if(id == 0xc5)ret=max32(ret,1902); // base64
		if(id == 0xc6)ret=max32(ret,1800); // coff
		if(id == 0xc7)ret=max32(ret,1508); // ext4
		if(id == 0xc8)ret=max32(ret,1507); // vmdk
		if(id == 0xcc)ret=max32(ret,1506); // vmdk
		if(id == 0xd0)ret=max32(ret, 921); // uefi
		if(id == 0xd2)ret=max32(ret, 918); // squashfs
		if(id == 0xd4)ret=max32(ret, 909); // apm
		if(id == 0xd5)ret=max32(ret, 906); // mslz
		// if(id == 0xdb)ret=max32(ret, 904); // mbr
		if(id == 0xe0)ret=max32(ret, 459); // udf
		if(id == 0xe5)ret=max32(ret, 452); // compound
		if(id == 0xe9)ret=max32(ret, 427); // chm
	}

	return ret;
}

int lzmaGuessVersion(){
	return guessed7zVersion;
}

#if 0
//call with CreateArchiver
GUID CLSID_CArchiveHandler=
	{0x23170F69,0x40C1,0x278A,
	{0x10,0x00,0x00,0x01,0x10,/*archive type*/0x07,0x00,0x00}};

//call with CreateCodec
GUID CLSID_CCodec=
	{0x23170F69,0x40C1,/*decoder:0x2790 encoder:0x2791*/0x2791,
	{0x08,0x01,0x04,0x00,0x00,0x00,0x00,0x00}}; //Codec ID in little endian
	//{0x01,0x01,0x03,0x00,0x00,0x00,0x00,0x00}}; //LZMA
	//{0x02,0x02,0x04,0x00,0x00,0x00,0x00,0x00}}; //BZip2
#endif

static int lzmaGUIDSetArchiver(void *g,unsigned char arctype){
	if(!g)return 1;
	GUID *guid=(GUID*)g;
	guid->Data1=0x23170F69;
	guid->Data2=0x40C1;
	guid->Data3=0x278A;
	guid->Data4[0]=0x10;
	guid->Data4[1]=0x00;
	guid->Data4[2]=0x00;
	guid->Data4[3]=0x01;
	guid->Data4[4]=0x10;
	guid->Data4[5]=arctype;
	guid->Data4[6]=0x00;
	guid->Data4[7]=0x00;
	return 0;
}

static int lzmaGUIDSetCoder(void *g,unsigned long long int codecid,int encode){
	if(!g||(encode!=0&&encode!=1))return 1;
	GUID *guid=(GUID*)g;
	guid->Data1=0x23170F69;
	guid->Data2=0x40C1;
	guid->Data3=0x2790|encode;
	int i=0;
	for(;i<8;i++,codecid>>=8)guid->Data4[i]=codecid&0xff;
	return 0;
}

static HRESULT SCompressCodecsInfoExternal22_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SCompressCodecsInfoExternal22_ *self = (SCompressCodecsInfoExternal22_*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static HRESULT SCompressCodecsInfoExternal23_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	LZMA_UNUSED SCompressCodecsInfoExternal23_ *self = (SCompressCodecsInfoExternal23_*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SCompressCodecsInfoExternal_AddRef(void* _self){
	LZMA_UNUSED SCompressCodecsInfoExternal23_ *self = (SCompressCodecsInfoExternal23_*)_self;
	return ++self->refs;
}

static u32 WINAPI SCompressCodecsInfoExternal_Release(void* _self){
	LZMA_UNUSED SCompressCodecsInfoExternal23_ *self = (SCompressCodecsInfoExternal23_*)_self;
	if(--self->refs==0){
		pSetCodecs23(NULL); // 22 or 23, whichever works
		free(self->vt);
		self->vt=NULL;
		if(self->vCodecs){
			size_t n = scv_size(self->vCodecs);
			for(size_t i=0; i<n; i++){
				FreeLibrary(*(HMODULE*)scv_at(self->vCodecs, i));
			}
			scv_delete(self->vCodecs);
			self->vCodecs = NULL;
		}
		if(self->vNumMethods)scv_delete(self->vNumMethods);
		self->vNumMethods=NULL;
		if(self->vGetMethodProperty)scv_delete(self->vGetMethodProperty);
		self->vGetMethodProperty=NULL;
		if(self->vCreateEncoder)scv_delete(self->vCreateEncoder);
		self->vCreateEncoder=NULL;
		if(self->vCreateDecoder)scv_delete(self->vCreateDecoder);
		self->vCreateDecoder=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SCompressCodecsInfoExternal_GetNumMethods(void* _self, u32 *numMethods){
	LZMA_UNUSED SCompressCodecsInfoExternal23_ *self = (SCompressCodecsInfoExternal23_*)_self;
	size_t n = scv_size(self->vNumMethods);
	*numMethods = 0;
	for(size_t i=0; i<n; i++){
		*numMethods += *(u32*)scv_at(self->vNumMethods, i);
	}
	return S_OK;
}

static HRESULT WINAPI SCompressCodecsInfoExternal_GetProperty(void* _self, u32 index, PROPID propID, PROPVARIANT *value){
	LZMA_UNUSED SCompressCodecsInfoExternal23_ *self = (SCompressCodecsInfoExternal23_*)_self;
	size_t n = scv_size(self->vNumMethods);
	for(size_t i=0; i<n; i++){
		u32 numMethods = *(u32*)scv_at(self->vNumMethods, i);
		if(numMethods>index){
			return (*(funcGetProperty*)scv_at(self->vGetMethodProperty, i))(index,propID,value);
		}
		index -= numMethods;
	}
	return E_FAIL;
}

static HRESULT WINAPI SCompressCodecsInfoExternal_CreateDecoder(void* _self, u32 index, const GUID *iid, void **coder){
	LZMA_UNUSED SCompressCodecsInfoExternal23_ *self = (SCompressCodecsInfoExternal23_*)_self;
	size_t n = scv_size(self->vNumMethods);
	for(size_t i=0; i<n; i++){
		u32 numMethods = *(u32*)scv_at(self->vNumMethods, i);
		if(numMethods>index){
			return (*(funcCreateDecoder*)scv_at(self->vCreateDecoder, i))(index,iid,coder);
		}
		index -= numMethods;
	}
	return E_FAIL;
}

static HRESULT WINAPI SCompressCodecsInfoExternal_CreateEncoder(void* _self, u32 index, const GUID *iid, void **coder){
	LZMA_UNUSED SCompressCodecsInfoExternal23_ *self = (SCompressCodecsInfoExternal23_*)_self;
	size_t n = scv_size(self->vNumMethods);
	for(size_t i=0; i<n; i++){
		u32 numMethods = *(u32*)scv_at(self->vNumMethods, i);
		if(numMethods>index){
			return (*(funcCreateEncoder*)scv_at(self->vCreateEncoder, i))(index,iid,coder);
		}
		index -= numMethods;
	}
	return E_FAIL;
}

unsigned long long FileTimeToUTC(const FILETIME in){
	return ((unsigned long long)in.dwHighDateTime*0x100000000+in.dwLowDateTime-0x19DB1DED53E8000)/10000000;
}

FILETIME UTCToFileTime(const unsigned long long UTC){
	FILETIME ftime;
	unsigned long long ll = UTC*10000000+0x19DB1DED53E8000;
	ftime.dwLowDateTime  = (DWORD) ll;
	ftime.dwHighDateTime = (DWORD)(ll >>32);
	return ftime;
}

#define ISetProperties_ ISetProperties22_
#define IInStream_ IInStream22_
#define IOutStream_ IOutStream22_
#define IInArchive_ IInArchive22_
#define IOutArchive_ IOutArchive22_
#define IArchiveOpenCallback_ IArchiveOpenCallback22_
#define IArchiveExtractCallback_ IArchiveExtractCallback22_
#define IArchiveUpdateCallback_ IArchiveUpdateCallback22_
#define ICompressCoder_ ICompressCoder22_
#define ICompressSetCoderProperties_ ICompressSetCoderProperties22_
#define ICompressCodecsInfo_ ICompressCodecsInfo22_
#define ICompressCodecsInfo_vt ICompressCodecsInfo22_vt
#define IHashers_ IHashers22_
#define scoder scoder22
#define pSetCodecs pSetCodecs22
#define pGetHashers pGetHashers22
#define SArchiveOpenCallbackPassword SArchiveOpenCallbackPassword22
#define SInStreamMem SInStreamMem22
#define SSequentialOutStreamMem SSequentialOutStreamMem22
#define SInStreamGeneric SInStreamGeneric22
#define SSequentialOutStreamGeneric SSequentialOutStreamGeneric22
#define MakeSArchiveOpenCallbackPassword MakeSArchiveOpenCallbackPassword22
#define MakeSInStreamMem MakeSInStreamMem22
#define MakeSSequentialOutStreamMem MakeSSequentialOutStreamMem22
#define MakeSInStreamGeneric MakeSInStreamGeneric22
#define MakeSSequentialOutStreamGeneric MakeSSequentialOutStreamGeneric22
#define SCompressCodecsInfoExternal_QueryInterface SCompressCodecsInfoExternal22_QueryInterface
#define lzmaShowInfos lzmaShowInfos22
#define lzmaCreateArchiver lzmaCreateArchiver22
#define lzmaDestroyArchiver lzmaDestroyArchiver22
#define lzmaOpenArchive lzmaOpenArchive22
#define lzmaCloseArchive lzmaCloseArchive22
#define lzmaGetArchiveFileNum lzmaGetArchiveFileNum22
#define lzmaGetArchiveFileProperty lzmaGetArchiveFileProperty22
#define lzmaExtractArchive lzmaExtractArchive22
#define lzmaUpdateArchive lzmaUpdateArchive22
#define lzmaCreateCoder lzmaCreateCoder22
#define lzmaDestroyCoder lzmaDestroyCoder22
#define lzmaCodeOneshot lzmaCodeOneshot22
#define lzmaCodeCallback lzmaCodeCallback22
#define lzmaLoadExternalCodecs lzmaLoadExternalCodecs22
#define lzmaUnloadExternalCodecs lzmaUnloadExternalCodecs22
#include "lzmasdk_23.h"
#undef ISetProperties_
#undef IInStream_
#undef IOutStream_
#undef IInArchive_
#undef IOutArchive_
#undef IArchiveOpenCallback_
#undef IArchiveExtractCallback_
#undef IArchiveUpdateCallback_
#undef ICompressCoder_
#undef ICompressSetCoderProperties_
#undef ICompressCodecsInfo_
#undef ICompressCodecsInfo_vt
#undef IHashers_
#undef scoder
#undef pSetCodecs
#undef pGetHashers
#undef SArchiveOpenCallbackPassword
#undef SInStreamMem
#undef SSequentialOutStreamMem
#undef SInStreamGeneric
#undef SSequentialOutStreamGeneric
#undef MakeSArchiveOpenCallbackPassword
#undef MakeSInStreamMem
#undef MakeSSequentialOutStreamMem
#undef MakeSInStreamGeneric
#undef MakeSSequentialOutStreamGeneric
#undef SCompressCodecsInfoExternal_QueryInterface
#undef lzmaShowInfos
#undef lzmaCreateArchiver 
#undef lzmaDestroyArchiver
#undef lzmaOpenArchive
#undef lzmaCloseArchive
#undef lzmaGetArchiveFileNum
#undef lzmaGetArchiveFileProperty
#undef lzmaExtractArchive
#undef lzmaUpdateArchive
#undef lzmaCreateCoder
#undef lzmaDestroyCoder
#undef lzmaCodeOneshot
#undef lzmaCodeCallback
#undef lzmaLoadExternalCodecs
#undef lzmaUnloadExternalCodecs

#define ISetProperties_ ISetProperties23_
#define IInStream_ IInStream23_
#define IOutStream_ IOutStream23_
#define IInArchive_ IInArchive23_
#define IOutArchive_ IOutArchive23_
#define IArchiveOpenCallback_ IArchiveOpenCallback23_
#define IArchiveExtractCallback_ IArchiveExtractCallback23_
#define IArchiveUpdateCallback_ IArchiveUpdateCallback23_
#define ICompressCoder_ ICompressCoder23_
#define ICompressSetCoderProperties_ ICompressSetCoderProperties23_
#define ICompressCodecsInfo_ ICompressCodecsInfo23_
#define ICompressCodecsInfo_vt ICompressCodecsInfo23_vt
#define IHashers_ IHashers23_
#define scoder scoder23
#define pSetCodecs pSetCodecs23
#define pGetHashers pGetHashers23
#define SArchiveOpenCallbackPassword SArchiveOpenCallbackPassword23
#define SInStreamMem SInStreamMem23
#define SSequentialOutStreamMem SSequentialOutStreamMem23
#define SInStreamGeneric SInStreamGeneric23
#define SSequentialOutStreamGeneric SSequentialOutStreamGeneric23
#define MakeSArchiveOpenCallbackPassword MakeSArchiveOpenCallbackPassword23
#define MakeSInStreamMem MakeSInStreamMem23
#define MakeSSequentialOutStreamMem MakeSSequentialOutStreamMem23
#define MakeSInStreamGeneric MakeSInStreamGeneric23
#define MakeSSequentialOutStreamGeneric MakeSSequentialOutStreamGeneric23
#define SCompressCodecsInfoExternal_QueryInterface SCompressCodecsInfoExternal23_QueryInterface
#define lzmaShowInfos lzmaShowInfos23
#define lzmaCreateArchiver lzmaCreateArchiver23
#define lzmaDestroyArchiver lzmaDestroyArchiver23
#define lzmaOpenArchive lzmaOpenArchive23
#define lzmaCloseArchive lzmaCloseArchive23
#define lzmaGetArchiveFileNum lzmaGetArchiveFileNum23
#define lzmaGetArchiveFileProperty lzmaGetArchiveFileProperty23
#define lzmaExtractArchive lzmaExtractArchive23
#define lzmaUpdateArchive lzmaUpdateArchive23
#define lzmaCreateCoder lzmaCreateCoder23
#define lzmaDestroyCoder lzmaDestroyCoder23
#define lzmaCodeOneshot lzmaCodeOneshot23
#define lzmaCodeCallback lzmaCodeCallback23
#define lzmaLoadExternalCodecs lzmaLoadExternalCodecs23
#define lzmaUnloadExternalCodecs lzmaUnloadExternalCodecs23
#include "lzmasdk_23.h"
#undef ISetProperties_
#undef IInStream_
#undef IOutStream_
#undef IInArchive_
#undef IOutArchive_
#undef IArchiveOpenCallback_
#undef IArchiveExtractCallback_
#undef IArchiveUpdateCallback_
#undef ICompressCoder_
#undef ICompressSetCoderProperties_
#undef ICompressCodecsInfo_
#undef ICompressCodecsInfo_vt
#undef IHashers_
#undef scoder
#undef pSetCodecs
#undef pGetHashers
#undef SArchiveOpenCallbackPassword
#undef SInStreamMem
#undef SSequentialOutStreamMem
#undef SInStreamGeneric
#undef SSequentialOutStreamGeneric
#undef MakeSArchiveOpenCallbackPassword
#undef MakeSInStreamMem
#undef MakeSSequentialOutStreamMem
#undef MakeSInStreamGeneric
#undef MakeSSequentialOutStreamGeneric
#undef SCompressCodecsInfoExternal_QueryInterface
#undef lzmaShowInfos
#undef lzmaCreateArchiver 
#undef lzmaDestroyArchiver
#undef lzmaOpenArchive
#undef lzmaCloseArchive
#undef lzmaGetArchiveFileNum
#undef lzmaGetArchiveFileProperty
#undef lzmaExtractArchive
#undef lzmaUpdateArchive
#undef lzmaCreateCoder
#undef lzmaDestroyCoder
#undef lzmaCodeOneshot
#undef lzmaCodeCallback
#undef lzmaLoadExternalCodecs
#undef lzmaUnloadExternalCodecs

int lzmaShowInfos(FILE *out){return guessed7zVersion>=2300 ? lzmaShowInfos23(out) : lzmaShowInfos22(out);}

int lzmaCreateArchiver(void **archiver,unsigned char arctype,int encode,int level){
	return guessed7zVersion>=2300 ? lzmaCreateArchiver23(archiver, arctype, encode, level) : lzmaCreateArchiver22(archiver, arctype, encode, level);
}
int lzmaDestroyArchiver(void **archiver,int encode){
	return guessed7zVersion>=2300 ? lzmaDestroyArchiver23(archiver, encode) : lzmaDestroyArchiver22(archiver, encode);
}
int lzmaOpenArchive(void *archiver,void *reader,const char *password,const char *fname){
	return guessed7zVersion>=2300 ? lzmaOpenArchive23(archiver, reader, password, fname) : lzmaOpenArchive22(archiver, reader, password, fname);
}
int lzmaCloseArchive(void *archiver){
	return guessed7zVersion>=2300 ? lzmaCloseArchive23(archiver) : lzmaCloseArchive22(archiver);
}
int lzmaGetArchiveFileNum(void *archiver,unsigned int *numItems){
	return guessed7zVersion>=2300 ? lzmaGetArchiveFileNum23(archiver, numItems) : lzmaGetArchiveFileNum22(archiver, numItems);
}
int lzmaGetArchiveFileProperty(void *archiver,unsigned int index,int kpid,PROPVARIANT *prop){
	return guessed7zVersion>=2300 ? lzmaGetArchiveFileProperty23(archiver, index, kpid, prop) : lzmaGetArchiveFileProperty22(archiver, index, kpid, prop);
}
int lzmaExtractArchive(void *archiver,const unsigned int* indices, unsigned int numItems, int testMode, void *callback){
	return guessed7zVersion>=2300 ? lzmaExtractArchive23(archiver, indices, numItems, testMode, callback) : lzmaExtractArchive22(archiver, indices, numItems, testMode, callback);
}
int lzmaUpdateArchive(void *archiver,void *writer,u32 numItems,void *callback){
	return guessed7zVersion>=2300 ? lzmaUpdateArchive23(archiver, writer, numItems, callback) : lzmaUpdateArchive22(archiver, writer, numItems, callback);
}

int lzmaCreateCoder(void **coder,unsigned long long int id,int encode,int level){
	return guessed7zVersion>=2300 ? lzmaCreateCoder23(coder, id, encode, level) : lzmaCreateCoder22(coder, id, encode, level);
}
int lzmaDestroyCoder(void **coder){
	return guessed7zVersion>=2300 ? lzmaDestroyCoder23(coder) : lzmaDestroyCoder22(coder);
}
int lzmaCodeOneshot(void *coder, const unsigned char *in, size_t isize, unsigned char *out, size_t *osize){
	return guessed7zVersion>=2300 ? lzmaCodeOneshot23(coder, in, isize, out, osize) : lzmaCodeOneshot22(coder, in, isize, out, osize);
}
int lzmaCodeCallback(void *coder, void *hin, tRead pRead_in, tClose pClose_in, void *hout, tWrite pWrite_out, tClose pClose_out){
	return guessed7zVersion>=2300 ? lzmaCodeCallback23(coder, hin, pRead_in, pClose_in, hout, pWrite_out, pClose_out) : lzmaCodeCallback22(coder, hin, pRead_in, pClose_in, hout, pWrite_out, pClose_out);
}

int lzmaLoadExternalCodecs(){return guessed7zVersion>=2300 ? lzmaLoadExternalCodecs23() : lzmaLoadExternalCodecs22();}
int lzmaUnloadExternalCodecs(){return guessed7zVersion>=2300 ? lzmaUnloadExternalCodecs23() : lzmaUnloadExternalCodecs22();}

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
HRESULT PropVariantClear(PROPVARIANT *pvar){
	if(!pvar)return E_INVALIDARG;
	if(pvar->vt == VT_BSTR)SysFreeString(pvar->bstrVal);
	memset(pvar,0,sizeof(PROPVARIANT));
	return S_OK;
}
BSTR SysAllocString(const OLECHAR *str){
	u32 len = wcslen(str);
	return SysAllocStringLen(str,len);
}
BSTR SysAllocStringLen(const OLECHAR *str,u32 len){
	u8 *x = (u8*)calloc(1,4+(len+1)*sizeof(OLECHAR)); // needs to be calloc.
	write32(x,len);
	BSTR ret = (BSTR)(x+4);
	if(str){
		int i=0;
		for(;i<len;i++)ret[i]=str[i];
	}
	return ret;
}
void SysFreeString(BSTR str){
	if(str)free(((u8*)str)-4);
}
u32 SysStringLen(BSTR str){
	if(!str)return 0;
	return read32(((u8*)str)-4);
}
#endif

#ifdef __cplusplus
}
#endif
