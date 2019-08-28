// LZMA SDK C interface
// basic idea from:
// https://gist.github.com/harvimt/9461046
// https://github.com/harvimt/pylib7zip

// STDMETHOD is virtual __stdcall (or __cdecl), so it can be called from C if the interface is properly aligned.

#include "lzma.h"
#include <string.h>

void write32(void *p, const unsigned int n){
	unsigned char *x=(unsigned char*)p;
	x[0]=n&0xff,x[1]=(n>>8)&0xff,x[2]=(n>>16)&0xff,x[3]=(n>>24)&0xff;
}

const IID IID_IUnknown_={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

const IID IID_ISequentialInStream_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x03,0x00,0x01,0x00,0x00}};
const IID IID_ISequentialOutStream_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x03,0x00,0x02,0x00,0x00}};
const IID IID_IInStream_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x03,0x00,0x03,0x00,0x00}};
const IID IID_IOutStream_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x03,0x00,0x04,0x00,0x00}};

const IID IID_ICompressCoder_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x04,0x00,0x05,0x00,0x00}};
const IID IID_ICompressSetCoderProperties_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x04,0x00,0x20,0x00,0x00}};
const IID IID_ICompressCodecsInfo_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x04,0x00,0x60,0x00,0x00}};

const IID IID_ICryptoGetTextPassword_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x05,0x00,0x10,0x00,0x00}};

const IID IID_IArchiveExtractCallback_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0x20,0x00,0x00}};
const IID IID_IInArchive_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0x60,0x00,0x00}};
const IID IID_IArchiveUpdateCallback_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0x80,0x00,0x00}};
const IID IID_IOutArchive_={0x23170F69,0x40C1,0x278A,{0x00,0x00,0x00,0x06,0x00,0xA0,0x00,0x00}};

static HRESULT WINAPI SInStreamFile_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SInStreamFile* self = (SInStreamFile*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SInStreamFile_AddRef(void* _self){
	SInStreamFile* self = (SInStreamFile*)_self;
	return ++self->refs;
}

static u32 WINAPI SInStreamFile_Release(void* _self){
	SInStreamFile* self = (SInStreamFile*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		if(self->f)fclose(self->f);
		self->f=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SInStreamFile_Read(void* _self, void *data, u32 size, u32 *processedSize){
	SInStreamFile* self = (SInStreamFile*)_self;
	u32 readlen = fread(data, 1, size, self->f);
	if(processedSize)*processedSize = readlen;
	return S_OK;
}

static HRESULT WINAPI SInStreamFile_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	SInStreamFile* self = (SInStreamFile*)_self;
	fseeko(self->f, offset, seekOrigin);
	if(newPosition)*newPosition = ftello(self->f);
	return S_OK;
}

bool WINAPI MakeSInStreamFile(SInStreamFile *self, const char *fname){
	self->f = fopen(fname,"rb");
	if(!self->f)return false;
	self->vt = (IInStream_vt*)calloc(1,sizeof(IInStream_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SInStreamFile_QueryInterface;
	self->vt->AddRef = SInStreamFile_AddRef;
	self->vt->Release = SInStreamFile_Release;
	self->vt->Read = SInStreamFile_Read;
	self->vt->Seek = SInStreamFile_Seek;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SInStreamMem_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SInStreamMem* self = (SInStreamMem*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SInStreamMem_AddRef(void* _self){
	SInStreamMem* self = (SInStreamMem*)_self;
	return ++self->refs;
}

static u32 WINAPI SInStreamMem_Release(void* _self){
	SInStreamMem* self = (SInStreamMem*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		mclose(self->m);
		self->m=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SInStreamMem_Read(void* _self, void *data, u32 size, u32 *processedSize){
	SInStreamMem* self = (SInStreamMem*)_self;
	u32 readlen = mread(data, size, self->m);
	if(processedSize)*processedSize = readlen;
	return S_OK;
}

static HRESULT WINAPI SInStreamMem_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	SInStreamMem* self = (SInStreamMem*)_self;
	u64 pos = mseek(self->m, offset, seekOrigin);
	if(newPosition)*newPosition = pos;
	return S_OK;
}

bool MakeSInStreamMem(SInStreamMem *self, void *p, const unsigned int size){
	self->m = mopen(p,size,NULL);
	if(!self->m)return false;
	self->vt = (IInStream_vt*)calloc(1,sizeof(IInStream_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SInStreamMem_QueryInterface;
	self->vt->AddRef = SInStreamMem_AddRef;
	self->vt->Release = SInStreamMem_Release;
	self->vt->Read = SInStreamMem_Read;
	self->vt->Seek = SInStreamMem_Seek;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SInStreamGeneric_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SInStreamGeneric* self = (SInStreamGeneric*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SInStreamGeneric_AddRef(void* _self){
	SInStreamGeneric* self = (SInStreamGeneric*)_self;
	return ++self->refs;
}

static u32 WINAPI SInStreamGeneric_Release(void* _self){
	SInStreamGeneric* self = (SInStreamGeneric*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		if(self->pClose)self->pClose(self->h);
		self->h=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SInStreamGeneric_Read(void* _self, void *data, u32 size, u32 *processedSize){
	SInStreamGeneric* self = (SInStreamGeneric*)_self;
	if(!self->h||!self->pRead)return E_FAIL;
	int readlen=self->pRead(self->h,(char*)data,size);
	if(processedSize)*processedSize=readlen;
	return S_OK;
}

static HRESULT WINAPI SInStreamGeneric_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	SInStreamGeneric* self = (SInStreamGeneric*)_self;
	if(!self->h||!self->pSeek)return E_FAIL;
	if(self->pSeek(self->h,offset,seekOrigin))return E_FAIL;//errno;
	if(newPosition){
		if(!self->pTell)return E_FAIL;
		*newPosition=self->pTell(self->h);
	}
	return S_OK;
}

bool MakeSInStreamGeneric(SInStreamGeneric *self, void *h, tRead pRead, tClose pClose, tSeek pSeek, tTell pTell){
	self->h = h;
	if(!self->h)return false;
	self->pRead = pRead;
	self->pSeek = pSeek;
	self->pClose = pClose;
	self->pTell = pTell;
	self->vt = (IInStream_vt*)calloc(1,sizeof(IInStream_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SInStreamGeneric_QueryInterface;
	self->vt->AddRef = SInStreamGeneric_AddRef;
	self->vt->Release = SInStreamGeneric_Release;
	self->vt->Read = SInStreamGeneric_Read;
	self->vt->Seek = SInStreamGeneric_Seek;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SOutStreamFile_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SOutStreamFile* self = (SOutStreamFile*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SOutStreamFile_AddRef(void* _self){
	SOutStreamFile* self = (SOutStreamFile*)_self;
	return ++self->refs;
}

static u32 WINAPI SOutStreamFile_Release(void* _self){
	SOutStreamFile* self = (SOutStreamFile*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		if(self->f)fclose(self->f);
		self->f=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SOutStreamFile_Write(void* _self, const void *data, u32 size, u32 *processedSize){
	SOutStreamFile* self = (SOutStreamFile*)_self;
	u32 writelen = fwrite(data, 1, size, self->f);
	if(processedSize)*processedSize = writelen;
	return S_OK;
}

static HRESULT WINAPI SOutStreamFile_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	SOutStreamFile* self = (SOutStreamFile*)_self;
	fseeko(self->f, offset, seekOrigin);
	if(newPosition)*newPosition = ftello(self->f);
	return S_OK;
}

static HRESULT WINAPI SOutStreamFile_SetSize(void* _self, u64 newSize){
	SOutStreamFile* self = (SOutStreamFile*)_self;
	ftruncate(fileno(self->f),newSize);
	return S_OK;
}

bool MakeSOutStreamFile(SOutStreamFile *self, const char *fname){
	/*
	self->f = fopen(fname,"ab");
	if(!self->f)return false;
	fclose(self->f);
	self->f = fopen(fname,"r+b");
	*/
	self->f = fopen(fname,"wb");
	if(!self->f)return false;
	self->vt = (IOutStream_vt*)calloc(1,sizeof(IOutStream_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SOutStreamFile_QueryInterface;
	self->vt->AddRef = SOutStreamFile_AddRef;
	self->vt->Release = SOutStreamFile_Release;
	self->vt->Write = SOutStreamFile_Write;
	self->vt->Seek = SOutStreamFile_Seek;
	self->vt->SetSize = SOutStreamFile_SetSize;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SSequentialOutStreamMem_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SSequentialOutStreamMem* self = (SSequentialOutStreamMem*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SSequentialOutStreamMem_AddRef(void* _self){
	SSequentialOutStreamMem* self = (SSequentialOutStreamMem*)_self;
	return ++self->refs;
}

static u32 WINAPI SSequentialOutStreamMem_Release(void* _self){
	SSequentialOutStreamMem* self = (SSequentialOutStreamMem*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		mclose(self->m);
		self->m=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SSequentialOutStreamMem_Write(void* _self, const void *data, u32 size, u32 *processedSize){
	SSequentialOutStreamMem* self = (SSequentialOutStreamMem*)_self;
	u32 writelen = mwrite(data, size, self->m);
	if(processedSize)*processedSize = writelen;
	return S_OK;
}

static HRESULT WINAPI SSequentialOutStreamMem_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	SSequentialOutStreamMem* self = (SSequentialOutStreamMem*)_self;
	u64 pos = mseek(self->m, offset, seekOrigin);
	if(newPosition)*newPosition = pos;
	return S_OK;
}

static HRESULT WINAPI SSequentialOutStreamMem_SetSize(void* _self, u64 newSize){
	SSequentialOutStreamMem* self = (SSequentialOutStreamMem*)_self;
	return E_FAIL; // this is why declared as Sequential.
}

bool MakeSSequentialOutStreamMem(SSequentialOutStreamMem *self, void *p, const unsigned int size){
	self->m = mopen(p,size,NULL);
	if(!self->m)return false;
	self->vt = (IOutStream_vt*)calloc(1,sizeof(IOutStream_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SSequentialOutStreamMem_QueryInterface;
	self->vt->AddRef = SSequentialOutStreamMem_AddRef;
	self->vt->Release = SSequentialOutStreamMem_Release;
	self->vt->Write = SSequentialOutStreamMem_Write;
	self->vt->Seek = SSequentialOutStreamMem_Seek;
	self->vt->SetSize = SSequentialOutStreamMem_SetSize;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SSequentialOutStreamGeneric_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SSequentialOutStreamGeneric* self = (SSequentialOutStreamGeneric*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SSequentialOutStreamGeneric_AddRef(void* _self){
	SSequentialOutStreamGeneric* self = (SSequentialOutStreamGeneric*)_self;
	return ++self->refs;
}

static u32 WINAPI SSequentialOutStreamGeneric_Release(void* _self){
	SSequentialOutStreamGeneric* self = (SSequentialOutStreamGeneric*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		if(self->pClose)self->pClose(self->h);
		self->h=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SSequentialOutStreamGeneric_Write(void* _self, const void *data, u32 size, u32 *processedSize){
	SSequentialOutStreamGeneric* self = (SSequentialOutStreamGeneric*)_self;
	if(!self->h||!self->pWrite)return E_FAIL;
	int readlen=self->pWrite(self->h,(char*)data,size);
	if(processedSize)*processedSize=readlen;
	return S_OK;
}

static HRESULT WINAPI SSequentialOutStreamGeneric_Seek(void* _self, s64 offset, u32 seekOrigin, u64 *newPosition){
	SSequentialOutStreamGeneric* self = (SSequentialOutStreamGeneric*)_self;
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
	SSequentialOutStreamGeneric* self = (SSequentialOutStreamGeneric*)_self;
	return E_FAIL;
}

bool MakeSSequentialOutStreamGeneric(SSequentialOutStreamGeneric *self, void *h, tWrite pWrite, tClose pClose){
	self->h = h;
	if(!self->h)return false;
	self->pWrite = pWrite;
	self->pClose = pClose;
	self->vt = (IOutStream_vt*)calloc(1,sizeof(IOutStream_vt));
	if(!self->vt)return false;
	self->vt->QueryInterface = SSequentialOutStreamGeneric_QueryInterface;
	self->vt->AddRef = SSequentialOutStreamGeneric_AddRef;
	self->vt->Release = SSequentialOutStreamGeneric_Release;
	self->vt->Write = SSequentialOutStreamGeneric_Write;
	self->vt->Seek = SSequentialOutStreamGeneric_Seek;
	self->vt->SetSize = SSequentialOutStreamGeneric_SetSize;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SCryptoGetTextPasswordFixed_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SCryptoGetTextPasswordFixed *self = (SCryptoGetTextPasswordFixed*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SCryptoGetTextPasswordFixed_AddRef(void* _self){
	SCryptoGetTextPasswordFixed *self = (SCryptoGetTextPasswordFixed*)_self;
	return ++self->refs;
}

static u32 WINAPI SCryptoGetTextPasswordFixed_Release(void* _self){
	SCryptoGetTextPasswordFixed *self = (SCryptoGetTextPasswordFixed*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		free(self->password);
		self->password=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SCryptoGetTextPasswordFixed_CryptoGetTextPassword(void *_self, BSTR* password){
	SCryptoGetTextPasswordFixed *self = (SCryptoGetTextPasswordFixed*)_self;
	if(self->password){
		u8* passwordStartAddr = (u8*)malloc(516);
		*password = (BSTR)(passwordStartAddr+4);
		write32(passwordStartAddr, mbstowcs(*password, self->password, 512));
	}
	return S_OK;
}

bool MakeSCryptoGetTextPasswordFixed(SCryptoGetTextPasswordFixed *self, const char *password){
	self->vt = (ICryptoGetTextPassword_vt*)calloc(1,sizeof(ICryptoGetTextPassword_vt));
	if(!self->vt)return false;
	self->password = NULL;
	if(password){
		self->password = (char*)malloc(strlen(password)+1);
		strcpy(self->password, password);
	}
	self->vt->QueryInterface = SCryptoGetTextPasswordFixed_QueryInterface;
	self->vt->AddRef = SCryptoGetTextPasswordFixed_AddRef;
	self->vt->Release = SCryptoGetTextPasswordFixed_Release;
	self->vt->CryptoGetTextPassword = SCryptoGetTextPasswordFixed_CryptoGetTextPassword;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SArchiveOpenCallbackPassword_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SArchiveOpenCallbackPassword *self = (SArchiveOpenCallbackPassword*)_self;
	if(!memcmp(iid,&IID_ICryptoGetTextPassword_,sizeof(GUID))){
		*out_obj = &self->setpassword;
		self->setpassword.vt->AddRef(&self->setpassword);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SArchiveOpenCallbackPassword_AddRef(void* _self){
	SArchiveOpenCallbackPassword *self = (SArchiveOpenCallbackPassword*)_self;
	return ++self->refs;
}

static u32 WINAPI SArchiveOpenCallbackPassword_Release(void* _self){
	SArchiveOpenCallbackPassword *self = (SArchiveOpenCallbackPassword*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
		self->setpassword.vt->Release(&self->setpassword);
	}
	return self->refs;
}

static HRESULT WINAPI SArchiveOpenCallbackPassword_SetTotal(void* _self, const u64 *files, const u64 *bytes){
	SArchiveOpenCallbackPassword *self = (SArchiveOpenCallbackPassword*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveOpenCallbackPassword_SetCompleted(void* _self, const u64 *files, const u64 *bytes){
	SArchiveOpenCallbackPassword *self = (SArchiveOpenCallbackPassword*)_self;
	return S_OK;
}

bool MakeSArchiveOpenCallbackPassword(SArchiveOpenCallbackPassword *self, const char *password){
	self->vt = (IArchiveOpenCallback_vt*)calloc(1,sizeof(IArchiveOpenCallback_vt));
	if(!self->vt)return false;
	MakeSCryptoGetTextPasswordFixed(&self->setpassword,password);
	self->vt->QueryInterface = SArchiveOpenCallbackPassword_QueryInterface;
	self->vt->AddRef = SArchiveOpenCallbackPassword_AddRef;
	self->vt->Release = SArchiveOpenCallbackPassword_Release;
	self->vt->SetTotal = SArchiveOpenCallbackPassword_SetTotal;
	self->vt->SetCompleted = SArchiveOpenCallbackPassword_SetCompleted;
	self->refs = 1;
	return true;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SArchiveExtractCallbackBare *self = (SArchiveExtractCallbackBare*)_self;
	if(!memcmp(iid,&IID_ICryptoGetTextPassword_,sizeof(GUID))){
		*out_obj = &self->setpassword;
		self->setpassword.vt->AddRef(&self->setpassword);
		return S_OK;
	}
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SArchiveExtractCallbackBare_AddRef(void* _self){
	SArchiveExtractCallbackBare *self = (SArchiveExtractCallbackBare*)_self;
	return ++self->refs;
}

static u32 WINAPI SArchiveExtractCallbackBare_Release(void* _self){
	SArchiveExtractCallbackBare *self = (SArchiveExtractCallbackBare*)_self;
	if(--self->refs==0){
		free(self->vt);
		self->vt=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_SetTotal(void* _self, const u64 total){
	SArchiveExtractCallbackBare *self = (SArchiveExtractCallbackBare*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_SetCompleted(void* _self, const u64 *completedValue){
	SArchiveExtractCallbackBare *self = (SArchiveExtractCallbackBare*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_GetStream(void* _self, u32 index, /*ISequentialOutStream_*/IOutStream_ **outStream, s32 askExtractMode){
	SArchiveExtractCallbackBare *self = (SArchiveExtractCallbackBare*)_self;
	*outStream = NULL;
	PROPVARIANT path;
	memset(&path,0,sizeof(PROPVARIANT));
	self->lastIndex = index;
	lzmaGetArchiveFileProperty(self->archiver, index, kpidPath, &path);
	//printf("%d\t%ls\n",index,path.bstrVal);
	IOutStream_* stream = (IOutStream_*)calloc(1,sizeof(SOutStreamFile));
	MakeSOutStreamFile((SOutStreamFile*)stream,"/dev/null");
	*outStream = stream; // WILL BE RELEASED AUTOMATICALLY.
	PropVariantClear(&path);
	return S_OK;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_PrepareOperation(void* _self, s32 askExtractMode){
	SArchiveExtractCallbackBare *self = (SArchiveExtractCallbackBare*)_self;
	return S_OK;
}

static HRESULT WINAPI SArchiveExtractCallbackBare_SetOperationResult(void* _self, s32 opRes){
	SArchiveExtractCallbackBare *self = (SArchiveExtractCallbackBare*)_self;
	return S_OK;
}

bool MakeSArchiveExtractCallbackBare(SArchiveExtractCallbackBare *self, IInArchive_ *archiver, const char *password){
	self->vt = (IArchiveExtractCallback_vt*)calloc(1,sizeof(IArchiveExtractCallback_vt));
	if(!self->vt)return false;
	MakeSCryptoGetTextPasswordFixed(&self->setpassword,password);
	self->archiver = archiver;
	self->lastIndex = -1;
	self->vt->QueryInterface = SArchiveExtractCallbackBare_QueryInterface;
	self->vt->AddRef = SArchiveExtractCallbackBare_AddRef;
	self->vt->Release = SArchiveExtractCallbackBare_Release;
	self->vt->SetTotal = SArchiveExtractCallbackBare_SetTotal;
	self->vt->SetCompleted = SArchiveExtractCallbackBare_SetCompleted;
	self->vt->GetStream = SArchiveExtractCallbackBare_GetStream;
	self->vt->PrepareOperation = SArchiveExtractCallbackBare_PrepareOperation;
	self->vt->SetOperationResult = SArchiveExtractCallbackBare_SetOperationResult;
	self->refs = 1;
	return true;
}



//Loading 7z.so
typedef HRESULT (WINAPI*funcCreateObject)(const GUID*,const GUID*,void**);
static funcCreateObject pCreateArchiver,pCreateCoder;
typedef HRESULT (WINAPI*funcSetCodecs)(ICompressCodecsInfo_ *compressCodecsInfo);
static funcSetCodecs pSetCodecs;
typedef HRESULT (WINAPI*funcGetNumMethods)(u32 *numMethods);
typedef HRESULT (WINAPI*funcGetProperty)(u32 index, PROPID propID, PROPVARIANT *value);
typedef HRESULT (WINAPI*funcCreateEncoder)(u32 index, const GUID *iid, void **coder);
typedef HRESULT (WINAPI*funcCreateDecoder)(u32 index, const GUID *iid, void **coder);

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	static HMODULE h7z=NULL;
#else
	static void *h7z=NULL;
#endif

int lzmaOpen7z(){
	if(lzma7zAlive())return 0;

	h7z=NULL;
#ifndef NODLOPEN
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	h7z=LoadLibraryA("C:\\Program Files\\7-Zip\\7z.dll");
	if(!h7z)h7z=LoadLibraryA("C:\\Program Files (x86)\\7-Zip\\7z.dll");
	if(!h7z)h7z=LoadLibraryA("7z.dll"); // last resort using PATH
#else
	h7z=LoadLibraryA("/usr/lib/p7zip/7z.so"); //Generic
	if(!h7z)h7z=LoadLibraryA("/usr/local/lib/p7zip/7z.so");
	if(!h7z)h7z=LoadLibraryA("/usr/libexec/p7zip/7z.so");
	if(!h7z)h7z=LoadLibraryA("/home/linuxbrew/.linuxbrew/lib/p7zip/7z.so");
	if(!h7z)h7z=LoadLibraryA("/opt/local/lib/p7zip/7z.so"); //MacPorts
	if(!h7z)h7z=LoadLibraryA("/sw/lib/p7zip/7z.so"); //Fink
	if(!h7z){
		char *home = getenv("HOME");
		if(home){
			char buf[256];
			sprintf(buf,"%s/.linuxbrew/lib/p7zip/7z.so",home);
			h7z=LoadLibraryA(buf);
		}
	}
	if(!h7z)h7z=LoadLibraryA("7z.so"); // last resort using LD_LIBRARY_PATH
	if(!h7z)h7z=LoadLibraryA("7z"); // for some cases such as '7z.dylib' (usually installed as 7z.so on all p7zip platforms, though).
#endif
#endif
	if(!h7z)return 1;

#ifndef NODLOPEN
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	pCreateArchiver=(funcCreateObject)GetProcAddress(h7z,"CreateObject");
	pCreateCoder=(funcCreateObject)GetProcAddress(h7z,"CreateObject");
	pSetCodecs=(funcSetCodecs)GetProcAddress(h7z,"SetCodecs");
#else
	pCreateArchiver=(funcCreateObject)GetProcAddress(h7z,"CreateArchiver");
	pCreateCoder=(funcCreateObject)GetProcAddress(h7z,"CreateCoder");
	pSetCodecs=(funcSetCodecs)GetProcAddress(h7z,"SetCodecs");
#endif
#endif
	if(!pCreateArchiver||!pCreateCoder){
#ifndef NODLOPEN
		FreeLibrary(h7z);
#endif
		h7z=NULL;
		return 2;
	}
	return 0; //now you can call 7z.so.
}
bool lzma7zAlive(){
	return h7z&&pCreateArchiver&&pCreateCoder;
}
int lzmaClose7z(){
	if(!h7z)return 1;
#ifndef NODLOPEN
	FreeLibrary(h7z);
#endif
	h7z=NULL;
	return 0;
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

//Archive API
int lzmaCreateArchiver(void **archiver,unsigned char arctype,int encode,int level){
	if(!h7z||!archiver)return -1;
	GUID CLSID_CArchiveHandler;
	lzmaGUIDSetArchiver(&CLSID_CArchiveHandler,arctype);
	if(pCreateArchiver(&CLSID_CArchiveHandler, encode?&IID_IOutArchive_:&IID_IInArchive_, archiver)!=S_OK){*archiver=NULL;return 1;}
	return 0;
}
int lzmaDestroyArchiver(void **archiver,int encode){
	if(!h7z||!archiver||!*archiver)return -1;
	if(encode)(*(IOutArchive_**)archiver)->vt->Release(*archiver);
	else (*(IInArchive_**)archiver)->vt->Release(*archiver);
	*archiver=NULL;
	return 0;
}

int lzmaOpenArchive(void *archiver,void *reader,const char *password){
	if(!h7z||!archiver)return -1;
	u64 maxCheckStartPosition=0;
	SArchiveOpenCallbackPassword scallback;
	MakeSArchiveOpenCallbackPassword(&scallback,password);
	int r=((IInArchive_*)archiver)->vt->Open(archiver, (IInStream_*)reader, &maxCheckStartPosition, (IArchiveOpenCallback_*)&scallback);
	scallback.vt->Release(&scallback);
	return r;
}
int lzmaCloseArchive(void *archiver){
	if(!h7z||!archiver)return -1;
	return ((IInArchive_*)archiver)->vt->Close(archiver);
}
int lzmaGetArchiveFileNum(void *archiver,unsigned int *numItems){
	if(!h7z||!archiver)return -1;
	return ((IInArchive_*)archiver)->vt->GetNumberOfItems(archiver, numItems);
}
int lzmaGetArchiveFileProperty(void *archiver,unsigned int index,int kpid,PROPVARIANT *prop){
	if(!h7z||!archiver)return -1;
	return ((IInArchive_*)archiver)->vt->GetProperty(archiver, index, kpid, prop);
}
int lzmaExtractArchive(void *archiver,const unsigned int* indices, unsigned int numItems, int testMode, void *callback){
	if(!h7z||!archiver)return -1;
	return ((IInArchive_*)archiver)->vt->Extract(archiver,indices,numItems,testMode,(IArchiveExtractCallback_*)callback);
}

//Coder API
int lzmaCreateCoder(void **coder,unsigned long long int id,int encode,int level){
	if(!h7z||!coder)return -1;
	if(level<1)level=1;
	if(level>9)level=9;

	GUID CLSID_CCodec;
	lzmaGUIDSetCoder(&CLSID_CCodec,id,encode);
	if(pCreateCoder(&CLSID_CCodec, &IID_ICompressCoder_, coder)!=S_OK){*coder=NULL;return 1;}
	if(!encode)return 0;

	ICompressSetCoderProperties_ *coderprop;
	(*(ICompressCoder_**)coder)->vt->QueryInterface(*coder, &IID_ICompressSetCoderProperties_, (void**)&coderprop);

#ifndef LZMA_SPECIAL_COMPRESSION_LEVEL
	PROPID ID[]={NCoderPropID_kLevel,NCoderPropID_kEndMarker,NCoderPropID_kDictionarySize};
	PROPVARIANT VAR[]={{VT_UI4,0,0,0,{0}},{VT_BOOL,0,0,0,{0}},{VT_UI4,0,0,0,{0}}};
	VAR[0].ulVal=level;
	VAR[1].boolVal=VARIANT_TRUE;
	VAR[2].ulVal=1<<24;
	coderprop->vt->SetCoderProperties(coderprop,ID,VAR,id==0x030101 ? 3 : 1);
#else
	//Deflate(64)
	if(id==0x040108||id==0x040109){
		//VT_UI4 NCoderPropID_kNumPasses
		//VT_UI4 NCoderPropID_kNumFastBytes (3 .. 255+2)
		//VT_UI4 NCoderPropID_kMatchFinderCycles
		//VT_UI4 NCoderPropID_kAlgorithm
		PROPID ID[]={NCoderPropID_kAlgorithm,NCoderPropID_kNumPasses,NCoderPropID_kNumFastBytes};
		PROPVARIANT VAR[]={{VT_UI4,0,0,0,{0}},{VT_UI4,0,0,0,{0}},{VT_UI4,0,0,0,{0}}};
		switch(level){
			case 9: VAR[0].ulVal=1,VAR[1].ulVal=10,VAR[2].ulVal=256;break;
			case 8: VAR[0].ulVal=1,VAR[1].ulVal=10,VAR[2].ulVal=128;break;
			case 7: VAR[0].ulVal=1,VAR[1].ulVal=3,VAR[2].ulVal=128;break;
			case 6: VAR[0].ulVal=1,VAR[1].ulVal=3,VAR[2].ulVal=64;break;
			case 5: VAR[0].ulVal=1,VAR[1].ulVal=1,VAR[2].ulVal=64;break;
			case 4: VAR[0].ulVal=0,VAR[1].ulVal=1,VAR[2].ulVal=32;break;
			case 3: VAR[0].ulVal=0,VAR[1].ulVal=1,VAR[2].ulVal=32;break;
			case 2: VAR[0].ulVal=0,VAR[1].ulVal=1,VAR[2].ulVal=32;break;
			case 1: VAR[0].ulVal=0,VAR[1].ulVal=1,VAR[2].ulVal=32;break;
		}
		coderprop->vt->SetCoderProperties(coderprop,ID,VAR,3);
	}
	//BZip2
	if(id==0x040202){
		//VT_UI4 NCoderPropID_kNumPasses
		//VT_UI4 NCoderPropID_kDictionarySize
		//VT_UI4 NCoderPropID_kNumThreads
		PROPID ID[]={NCoderPropID_kNumPasses,NCoderPropID_kDictionarySize};
		PROPVARIANT VAR[]={{VT_UI4,0,0,0,{0}},{VT_UI4,0,0,0,{0}}};
		switch(level){
			case 9: VAR[0].ulVal=9,VAR[1].ulVal=900000;break;
			case 8: VAR[0].ulVal=7,VAR[1].ulVal=900000;break;
			case 7: VAR[0].ulVal=2,VAR[1].ulVal=900000;break;
			case 6: VAR[0].ulVal=1,VAR[1].ulVal=900000;break;
			case 5: VAR[0].ulVal=1,VAR[1].ulVal=900000;break;
			case 4: VAR[0].ulVal=1,VAR[1].ulVal=700000;break;
			case 3: VAR[0].ulVal=1,VAR[1].ulVal=500000;break;
			case 2: VAR[0].ulVal=1,VAR[1].ulVal=300000;break;
			case 1: VAR[0].ulVal=1,VAR[1].ulVal=100000;break;
		}
		coderprop->vt->SetCoderProperties(coderprop,ID,VAR,2);
	}
	//PPMD
	if(id==0x030401){
		//VT_UI4 NCoderPropID_kUsedMemorySize
		//VT_UI4 NCoderPropID_kOrder
		PROPID ID[]={NCoderPropID_kUsedMemorySize,NCoderPropID_kOrder};
		PROPVARIANT VAR[]={{VT_UI4,0,0,0,{0}},{VT_UI4,0,0,0,{0}}};
		VAR[0].ulVal=1 << (19 + (level > 8 ? 8 : level));
		VAR[1].ulVal=3 + level;
		coderprop->vt->SetCoderProperties(coderprop,ID,VAR,2);
	}
	//LZMA
	if(id==0x030101){
		//VT_BSTR NCoderPropID_kMatchFinder
		//VT_UI4  NCoderPropID_kNumFastBytes (5 .. 2+8+8+256-1)
		//VT_UI4  NCoderPropID_kMatchFinderCycles
		//VT_UI4  NCoderPropID_kAlgorithm
		//VT_UI4  NCoderPropID_kDictionarySize
		//VT_UI4  NCoderPropID_kPosStateBits
		//VT_UI4  NCoderPropID_kLitPosBits
		//VT_UI4  NCoderPropID_kLitContextBits
		//VT_UI4  NCoderPropID_kNumThreads
		//VT_BOOL NCoderPropID_kEndMarker (not available in LZMA2)
		PROPID ID[]={
			NCoderPropID_kEndMarker,
			NCoderPropID_kMatchFinder,
			NCoderPropID_kAlgorithm,
			NCoderPropID_kNumFastBytes,
			NCoderPropID_kDictionarySize
		};
		PROPVARIANT VAR[]={{VT_BOOL,0,0,0,{0}},{VT_BSTR,0,0,0,{0}},{VT_UI4,0,0,0,{0}},{VT_UI4,0,0,0,{0}},{VT_UI4,0,0,0,{0}}};
		VAR[0].boolVal=VARIANT_TRUE;
		switch(level){
			case 9: VAR[1].bstrVal=L"BT4",VAR[2].ulVal=1,VAR[3].ulVal=256,VAR[4].ulVal=1<<24;break;//26
			case 8: VAR[1].bstrVal=L"BT4",VAR[2].ulVal=1,VAR[3].ulVal=128,VAR[4].ulVal=1<<24;break;//25
			case 7: VAR[1].bstrVal=L"BT4",VAR[2].ulVal=1,VAR[3].ulVal=64,VAR[4].ulVal=1<<24;break;//25
			case 6: VAR[1].bstrVal=L"BT4",VAR[2].ulVal=1,VAR[3].ulVal=64,VAR[4].ulVal=1<<24;break;
			case 5: VAR[1].bstrVal=L"BT4",VAR[2].ulVal=1,VAR[3].ulVal=32,VAR[4].ulVal=1<<24;break;
			case 4: VAR[1].bstrVal=L"HC4",VAR[2].ulVal=0,VAR[3].ulVal=32,VAR[4].ulVal=1<<24;break;//20
			case 3: VAR[1].bstrVal=L"HC4",VAR[2].ulVal=0,VAR[3].ulVal=32,VAR[4].ulVal=1<<24;break;//20
			case 2: VAR[1].bstrVal=L"HC4",VAR[2].ulVal=0,VAR[3].ulVal=32,VAR[4].ulVal=1<<24;break;//16
			case 1: VAR[1].bstrVal=L"HC4",VAR[2].ulVal=0,VAR[3].ulVal=32,VAR[4].ulVal=1<<24;break;//16
		}
		coderprop->vt->SetCoderProperties(coderprop,ID,VAR,5);
	}
	//LZMA2
	if(id==0x21){
		//VT_UI4 NCoderPropID_kBlockSize
		//left blank intentionally.
	}
#endif
	return 0;
}
int lzmaDestroyCoder(void **coder){
	if(!h7z||!coder||!*coder)return -1;
	(*(ICompressCoder_**)coder)->vt->Release(*coder);
	*coder=NULL;
	return 0;
}

static int mread2(void *h, char *p, int n){return mread(p,n,(memstream*)h);}
static int mwrite2(void *h, const char *p, int n){return mwrite(p,n,(memstream*)h);}
static int mclose2(void *h){return 0;}
int lzmaCodeOneshot(void *coder, unsigned char *in, size_t isize, unsigned char *out, size_t *osize){
		if(!h7z||!in||!out||!osize||!*osize)return -1;
		//unsigned long long int dummy=0;
		SInStreamMem sin;
		MakeSInStreamMem(&sin,in,isize);
		SSequentialOutStreamMem sout;
		MakeSSequentialOutStreamMem(&sout,out,*osize);
		HRESULT r=((ICompressCoder_*)coder)->vt->Code(coder,(IInStream_*)&sin, (IOutStream_*)&sout, (UINT64*)&isize, (UINT64*)osize, NULL);
		sout.vt->Seek(&sout,0,SEEK_CUR,(u64*)osize);
		sin.vt->Release(&sin);
		sout.vt->Release(&sout);
		if(r!=S_OK)return r;
		return 0;
}
int lzmaCodeCallback(void *coder, void *hin, tRead pRead_in, tClose pClose_in, void *hout, tWrite pWrite_out, tClose pClose_out){
//decoder isn't working here...
		if(!h7z||!hin||!hout)return -1;
		unsigned long long int isize=0xffffffffUL,osize=0xffffffffUL; //some_big_size... lol
		SInStreamGeneric sin;
		MakeSInStreamGeneric(&sin,hin,pRead_in,pClose_in,NULL,NULL);
		SSequentialOutStreamGeneric sout;
		MakeSSequentialOutStreamGeneric(&sout,hout,pWrite_out,pClose_out);
		HRESULT r=((ICompressCoder_*)coder)->vt->Code(coder,(IInStream_*)&sin, (IOutStream_*)&sout, (UINT64*)&isize, (UINT64*)osize, NULL);
		sin.vt->Release(&sin);
		sout.vt->Release(&sout);
		if(r!=S_OK)return r;
		return 0;
}

typedef struct{
	ICompressCodecsInfo_vt *vt;
	u32 refs;

	// todo make this array to support multiple codecs (without std::vector, how can I?).
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	HMODULE hRar;
#else
	void *hRar;
#endif
	funcGetNumMethods pGetNumMethods;
	funcGetProperty pGetProperty;
	funcCreateDecoder pCreateDecoder;
	funcCreateEncoder pCreateEncoder;
} SCompressCodecsInfoRar;
static SCompressCodecsInfoRar scoder; // for now expect to be placed BSS.

static HRESULT SCompressCodecsInfoRar_QueryInterface(void* _self, const GUID* iid, void** out_obj){
	SCompressCodecsInfoRar *self = (SCompressCodecsInfoRar*)_self;
	*out_obj = NULL;
	return E_NOINTERFACE;
}

static u32 WINAPI SCompressCodecsInfoRar_AddRef(void* _self){
	SCompressCodecsInfoRar *self = (SCompressCodecsInfoRar*)_self;
	return ++self->refs;
}

static u32 WINAPI SCompressCodecsInfoRar_Release(void* _self){
	SCompressCodecsInfoRar *self = (SCompressCodecsInfoRar*)_self;
	if(--self->refs==0){
		pSetCodecs(NULL);
		free(self->vt);
		self->vt=NULL;
		self->pGetNumMethods=NULL;
		self->pGetProperty=NULL;
		self->pCreateEncoder=NULL;
		self->pCreateDecoder=NULL;
		if(self->hRar)FreeLibrary(self->hRar);
		self->hRar=NULL;
	}
	return self->refs;
}

static HRESULT WINAPI SCompressCodecsInfoRar_GetNumMethods(void* _self, u32 *numMethods){
	SCompressCodecsInfoRar *self = (SCompressCodecsInfoRar*)_self;
	return self->pGetNumMethods(numMethods);
}

static HRESULT WINAPI SCompressCodecsInfoRar_GetProperty(void* _self, u32 index, PROPID propID, PROPVARIANT *value){
	SCompressCodecsInfoRar *self = (SCompressCodecsInfoRar*)_self;
	return self->pGetProperty(index,propID,value);
}

static HRESULT WINAPI SCompressCodecsInfoRar_CreateDecoder(void* _self, u32 index, const GUID *iid, void **coder){
	SCompressCodecsInfoRar *self = (SCompressCodecsInfoRar*)_self;
	return self->pCreateDecoder(index,iid,coder);
}

static HRESULT WINAPI SCompressCodecsInfoRar_CreateEncoder(void* _self, u32 index, const GUID *iid, void **coder){
	SCompressCodecsInfoRar *self = (SCompressCodecsInfoRar*)_self;
	return self->pCreateEncoder(index,iid,coder);
}

int lzmaLoadUnrar(){
	if(!pSetCodecs)return E_FAIL;
	if(!scoder.refs){
#ifndef NODLOPEN
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
		//Rar codecs are bundled inside 7z.dll.
#else
		scoder.hRar=LoadLibraryA("/usr/lib/p7zip/Codecs/Rar.so"); //Generic
		if(!scoder.hRar)scoder.hRar=LoadLibraryA("/usr/local/lib/p7zip/Codecs/Rar.so");
		if(!scoder.hRar)scoder.hRar=LoadLibraryA("/usr/libexec/p7zip/Codecs/Rar.so");
		if(!scoder.hRar)scoder.hRar=LoadLibraryA("/home/linuxbrew/.linuxbrew/lib/p7zip/Codecs/Rar.so");
		if(!scoder.hRar)scoder.hRar=LoadLibraryA("/opt/local/lib/p7zip/Codecs/Rar.so"); //MacPorts
		if(!scoder.hRar)scoder.hRar=LoadLibraryA("/sw/lib/p7zip/Codecs/Rar.so"); //Fink
		if(!scoder.hRar){
			char *home = getenv("HOME");
			if(home){
				char buf[256];
				sprintf(buf,"%s/.linuxbrew/lib/p7zip/Codecs/Rar.so",home);
				scoder.hRar=LoadLibraryA(buf);
			}
		}
		if(!scoder.hRar)h7z=LoadLibraryA("Rar.so"); // last resort using LD_LIBRARY_PATH
		if(!scoder.hRar)h7z=LoadLibraryA("Rar"); // for some cases such as '7z.dylib' (usually installed as 7z.so on all p7zip platforms, though).
		if(!scoder.hRar)return E_FAIL;
		scoder.vt = (ICompressCodecsInfo_vt*)calloc(1,sizeof(ICompressCodecsInfo_vt));
		scoder.vt->QueryInterface = SCompressCodecsInfoRar_QueryInterface;
		scoder.vt->AddRef = SCompressCodecsInfoRar_AddRef;
		scoder.vt->Release = SCompressCodecsInfoRar_Release;
		scoder.vt->GetNumMethods = SCompressCodecsInfoRar_GetNumMethods;
		scoder.vt->GetProperty = SCompressCodecsInfoRar_GetProperty;
		scoder.vt->CreateDecoder = SCompressCodecsInfoRar_CreateDecoder;
		scoder.vt->CreateEncoder = SCompressCodecsInfoRar_CreateEncoder;
		scoder.pGetNumMethods = (funcGetNumMethods)GetProcAddress(scoder.hRar,"GetNumberOfMethods");
		scoder.pGetProperty = (funcGetProperty)GetProcAddress(scoder.hRar,"GetMethodProperty");
		scoder.pCreateDecoder = (funcCreateDecoder)GetProcAddress(scoder.hRar,"CreateDecoder");
		scoder.pCreateEncoder = (funcCreateEncoder)GetProcAddress(scoder.hRar,"CreateEncoder");
		pSetCodecs((ICompressCodecsInfo_*)&scoder);
#endif
		return E_FAIL;
#endif
	}
	scoder.refs++;
	return S_OK;
}

int lzmaUnloadUnrar(){
	scoder.vt->Release(&scoder);
	return 0;
}

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
HRESULT PropVariantClear(PROPVARIANT *pvar){
	if(!pvar)return E_INVALIDARG;
	if(pvar->vt == VT_BSTR)free(((u8*)pvar->bstrVal)-4);
	memset(pvar,0,sizeof(PROPVARIANT));
	return S_OK;
}
#endif
