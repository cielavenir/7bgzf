static int lzmaShowInfos(FILE *out){
	if(!lzma7zAlive())return 1;
	if(!out)out = stderr;
	char strbuf[768];

	lzmaGet7zFileName(strbuf, 768);
	fprintf(out,"7-zip implementation: %s (guessed version: %04d (or newer))\n\n", strbuf, guessed7zVersion);

	fprintf(out, "Formats:\n");
	u32 numFormats;
	pGetNumFormats(&numFormats);
	for(u32 i=0;i<numFormats;i++){
		PROPVARIANT value;
		pGetHandlerProperty2(i, NHandlerPropID_kName, &value);
		wcstombs(strbuf, value.bstrVal, 768);
		fprintf(out, "%s\t", strbuf);
		if(strlen(strbuf)<8)fputc('\t', out);
		// PropVariantClear(&value); // cleared externally
		pGetHandlerProperty2(i, NHandlerPropID_kClassID, &value);
		fprintf(out, "%d (0x%02x)\t", ((u8*)value.bstrVal)[13], ((u8*)value.bstrVal)[13]);
		pGetHandlerProperty2(i, NHandlerPropID_kUpdate, &value);
		fputc(value.boolVal ? 'C' : '-', out);
		pGetHandlerProperty2(i, NHandlerPropID_kFlags, &value);
		// CPP/7zip/UI/Console/Main.cpp
		for(u32 j=0;j<14;j++)fputc((value.uintVal&(1<<j)) ? "KSNFMGOPBELHXC"[j] : '-', out);
		pGetHandlerProperty2(i, NHandlerPropID_kExtension, &value);
		wcstombs(strbuf, value.bstrVal, 768);
		fprintf(out, "\t%s", strbuf);
		//pGetHandlerProperty2(i, NHandlerPropID_kSignature, &value);
		//wcstombs(strbuf, value.bstrVal, 768);
		//fprintf(out, "\t%s", strbuf);
		fprintf(out, "\n");
	}

	fprintf(out, "Codecs:\n");
	u32 numMethods;
	pGetNumMethods(&numMethods);
	for(u32 i=0;i<numMethods;i++){
		PROPVARIANT value;
		pGetMethodProperty(i, NMethodPropID_kName, &value);
		wcstombs(strbuf, value.bstrVal, 768);
		fprintf(out, "%s\t", strbuf);
		if(strlen(strbuf)<8)fputc('\t', out);
		pGetMethodProperty(i, NMethodPropID_kID, &value);
		fprintf(out, "0x%08x\t", value.uintVal);
		pGetMethodProperty(i, NMethodPropID_kEncoderIsAssigned, &value);
		fputc(value.boolVal ? 'E' : '-', out);
		pGetMethodProperty(i, NMethodPropID_kDecoderIsAssigned, &value);
		fputc(value.boolVal ? 'D' : '-', out);
		HRESULT r = pGetMethodProperty(i, NMethodPropID_kIsFilter, &value);
		fputc(r ? '*' : value.boolVal ? 'F' : '-', out);
		//r = pGetMethodProperty(i, NMethodPropID_kDescription, &value);
		//if(!r){
		//	wcstombs(strbuf, value.bstrVal, 768);
		//	fprintf(out, "\t%s", strbuf);
		//}
		fprintf(out, "\n");
	}
	if(scoder.vt){
		SCompressCodecsInfoExternal_GetNumMethods(&scoder, &numMethods);
		for(u32 i=0;i<numMethods;i++){
			PROPVARIANT value;
			SCompressCodecsInfoExternal_GetProperty(&scoder, i, NMethodPropID_kName, &value);
			wcstombs(strbuf, value.bstrVal, 768);
			fprintf(out, "%s\t", strbuf);
			if(strlen(strbuf)<8)fputc('\t', out);
			SCompressCodecsInfoExternal_GetProperty(&scoder, i, NMethodPropID_kID, &value);
			fprintf(out, "0x%08x\t", value.uintVal);
			SCompressCodecsInfoExternal_GetProperty(&scoder, i, NMethodPropID_kEncoderIsAssigned, &value);
			fputc(value.boolVal ? 'E' : '-', out);
			SCompressCodecsInfoExternal_GetProperty(&scoder, i, NMethodPropID_kDecoderIsAssigned, &value);
			fputc(value.boolVal ? 'D' : '-', out);
			HRESULT r = SCompressCodecsInfoExternal_GetProperty(&scoder, i, NMethodPropID_kIsFilter, &value);
			fputc(r ? '*' : value.boolVal ? 'F' : '-', out);
			fprintf(out, "\n");
		}
	}

	fprintf(out, "Hashers:\n");
	IHashers_ *hashers;
	pGetHashers(&hashers);
	u32 numHashers = hashers->vt->GetNumHashers(hashers);
	for(u32 i=0;i<numHashers;i++){
		PROPVARIANT value;
		hashers->vt->GetHasherProp(hashers, i, NMethodPropID_kName, &value);
		wcstombs(strbuf, value.bstrVal, 768);
		fprintf(out, "%s\t", strbuf);
		if(strlen(strbuf)<8)fputc('\t', out);
		hashers->vt->GetHasherProp(hashers, i, NMethodPropID_kID, &value);
		fprintf(out, "0x%08x\t", value.uintVal);
		hashers->vt->GetHasherProp(hashers, i, NMethodPropID_kDigestSize, &value);
		fprintf(out, "%d", value.uintVal);
		//r = hashers->vt->GetHasherProperty(hashers, i, NMethodPropID_kDescription, &value);
		//if(!r){
		//	wcstombs(strbuf, value.bstrVal, 768);
		//	fprintf(out, "\t%s", strbuf);
		//}
		fprintf(out, "\n");
	}
	hashers->vt->Release(hashers);

	return 0;
}

//Archive API
static int lzmaCreateArchiver(void **archiver,unsigned char arctype,int encode,int level){
	if(!h7z||!archiver)return -1;
	GUID CLSID_CArchiveHandler;
	lzmaGUIDSetArchiver(&CLSID_CArchiveHandler,arctype);
	if(pCreateArchiver(&CLSID_CArchiveHandler, encode?&IID_IOutArchive_:&IID_IInArchive_, archiver)!=S_OK){*archiver=NULL;return 1;}
	if(encode && level>=0){
		// there are massive options in 7-zip; just support -mx=N for now.
		ISetProperties_ *setprop;
		(*(IOutArchive_**)archiver)->vt->QueryInterface(*archiver, &IID_ISetProperties_, (void**)&setprop);
		const wchar_t* OPTS[]={L"x"};
		PROPVARIANT VAR[]={{VT_UI4,0,0,0,{0}}};
		VAR[0].uintVal=level;
		setprop->vt->SetProperties(setprop,OPTS,VAR,1);
	}
	return 0;
}
static int lzmaDestroyArchiver(void **archiver,int encode){
	if(!h7z||!archiver||!*archiver)return -1;
	if(encode)(*(IOutArchive_**)archiver)->vt->Release(*archiver);
	else (*(IInArchive_**)archiver)->vt->Release(*archiver);
	*archiver=NULL;
	return 0;
}

static int lzmaOpenArchive(void *archiver,void *reader,const char *password,const char *fname){
	if(!h7z||!archiver)return -1;
	u64 maxCheckStartPosition=0;
	SArchiveOpenCallbackPassword scallback;
	MakeSArchiveOpenCallbackPassword(&scallback,password,fname);
	int r=((IInArchive_*)archiver)->vt->Open(archiver, (IInStream_*)reader, &maxCheckStartPosition, (IArchiveOpenCallback_*)&scallback);
	scallback.vt->Release(&scallback);
	return r;
}
static int lzmaCloseArchive(void *archiver){
	if(!h7z||!archiver)return -1;
	return ((IInArchive_*)archiver)->vt->Close(archiver);
}
static int lzmaGetArchiveFileNum(void *archiver,unsigned int *numItems){
	if(!h7z||!archiver)return -1;
	return ((IInArchive_*)archiver)->vt->GetNumberOfItems(archiver, numItems);
}
static int lzmaGetArchiveFileProperty(void *archiver,unsigned int index,int kpid,PROPVARIANT *prop){
	if(!h7z||!archiver)return -1;
	return ((IInArchive_*)archiver)->vt->GetProperty(archiver, index, kpid, prop);
}
static int lzmaExtractArchive(void *archiver,const unsigned int* indices, unsigned int numItems, int testMode, void *callback){
	if(!h7z||!archiver)return -1;
	return ((IInArchive_*)archiver)->vt->Extract(archiver,indices,numItems,testMode,(IArchiveExtractCallback_*)callback);
}
static int lzmaUpdateArchive(void *archiver,void *writer,u32 numItems,void *callback){
	if(!h7z||!archiver)return -1;
	return ((IOutArchive_*)archiver)->vt->UpdateItems(archiver,(IOutStream_*)writer,numItems,(IArchiveUpdateCallback_*)callback);
}

//Coder API
static int lzmaCreateCoder(void **coder,unsigned long long int id,int encode,int level){
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
	VAR[0].uintVal=level;
	VAR[1].boolVal=VARIANT_TRUE;
	VAR[2].uintVal=1<<24;
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
static int lzmaDestroyCoder(void **coder){
	if(!h7z||!coder||!*coder)return -1;
	(*(ICompressCoder_**)coder)->vt->Release(*coder);
	*coder=NULL;
	return 0;
}

//static int mread2(void *h, char *p, int n){return mread(p,n,(memstream*)h);}
//static int mwrite2(void *h, const char *p, int n){return mwrite(p,n,(memstream*)h);}
//static int mclose2(void *h){return 0;}
static int lzmaCodeOneshot(void *coder, const unsigned char *in, size_t isize, unsigned char *out, size_t *osize){
		if(!h7z||!in||!out||!osize||!*osize)return -1;
		//unsigned long long int dummy=0;
		SInStreamMem sin;
		MakeSInStreamMem(&sin,(void*)in,isize);
		SSequentialOutStreamMem sout;
		MakeSSequentialOutStreamMem(&sout,out,*osize);
		HRESULT r=((ICompressCoder_*)coder)->vt->Code(coder,(IInStream_*)&sin, (IOutStream_*)&sout, (UINT64*)&isize, (UINT64*)osize, NULL);
		sout.vt->Seek(&sout,0,SEEK_CUR,(u64*)osize);
		sin.vt->Release(&sin);
		sout.vt->Release(&sout);
		if(r!=S_OK)return r;
		return 0;
}
static int lzmaCodeCallback(void *coder, void *hin, tRead pRead_in, tClose pClose_in, void *hout, tWrite pWrite_out, tClose pClose_out){
//decoder isn't working here...
		if(!h7z||!hin||!hout)return -1;
		unsigned long long int isize=0xffffffffUL,osize=0xffffffffUL; //some_big_size... lol
		SInStreamGeneric sin;
		MakeSInStreamGeneric(&sin,hin,pRead_in,pClose_in,NULL,NULL);
		SSequentialOutStreamGeneric sout;
		MakeSSequentialOutStreamGeneric(&sout,hout,pWrite_out,pClose_out);
		HRESULT r=((ICompressCoder_*)coder)->vt->Code(coder,(IInStream_*)&sin, (IOutStream_*)&sout, (UINT64*)&isize, (UINT64*)&osize, NULL);
		sin.vt->Release(&sin);
		sout.vt->Release(&sout);
		if(r!=S_OK)return r;
		return 0;
}

static int lzmaLoadExternalCodecs(){
	if(!lzma7zAlive())return E_FAIL;
	if(!pSetCodecs)return E_FAIL;
	if(!scoder.refs){
#ifdef NODLOPEN
		return E_FAIL;
#else
		scoder.vCodecs = scv_new(sizeof(HMODULE), 0);
		scoder.vNumMethods = scv_new(sizeof(u32), 0);
		scoder.vGetMethodProperty = scv_new(sizeof(void*), 0);
		scoder.vCreateDecoder = scv_new(sizeof(void*), 0);
		scoder.vCreateEncoder = scv_new(sizeof(void*), 0);
		{
			char path[768];
#if 0
//defined(DL_ANDROID)
			GetModuleFileNameA(pCreateArchiver,path,768);
#else
			GetModuleFileNameA(h7z,path,768);
#endif
			int i=strlen(path)-1;
			for(;i>=0;i--)if(path[i]=='/'||path[i]=='\\')break;
			i+=1;
			strcpy(path+i,"Codecs/");
			char *basename = path+strlen(path);
			DIR *d=opendir(path);
			if(d){
				struct dirent *ent;
				while(ent=readdir(d)){
					strcpy(basename,ent->d_name);
					HMODULE h=LoadLibraryA(path);
					if(h){
						funcGetNumMethods pGetNumMethods = (funcGetNumMethods)GetProcAddress(h,"GetNumberOfMethods");
						funcGetProperty pGetMethodProperty = (funcGetProperty)GetProcAddress(h,"GetMethodProperty");
						funcCreateDecoder pCreateDecoder = (funcCreateDecoder)GetProcAddress(h,"CreateDecoder");
						funcCreateEncoder pCreateEncoder = (funcCreateEncoder)GetProcAddress(h,"CreateEncoder");
						if(pGetNumMethods && pGetMethodProperty && pCreateDecoder && pCreateEncoder){
							u32 numMethods = 0;
							HRESULT ret = pGetNumMethods(&numMethods);
							if(ret == S_OK){
								scv_push_back(scoder.vCodecs, &h);
								scv_push_back(scoder.vNumMethods, &numMethods);
								scv_push_back(scoder.vGetMethodProperty, &pGetMethodProperty);
								scv_push_back(scoder.vCreateDecoder, &pCreateDecoder);
								scv_push_back(scoder.vCreateEncoder, &pCreateEncoder);
							}else{
								FreeLibrary(h);
							}
						}else{
							FreeLibrary(h);
						}
					}
				}
			}
		}
		scoder.vt = (ICompressCodecsInfo_vt*)calloc(1,sizeof(ICompressCodecsInfo_vt));
		scoder.vt->QueryInterface = SCompressCodecsInfoExternal_QueryInterface;
		scoder.vt->AddRef = SCompressCodecsInfoExternal_AddRef;
		scoder.vt->Release = SCompressCodecsInfoExternal_Release;
		scoder.vt->GetNumMethods = SCompressCodecsInfoExternal_GetNumMethods;
		scoder.vt->GetProperty = SCompressCodecsInfoExternal_GetProperty;
		scoder.vt->CreateDecoder = SCompressCodecsInfoExternal_CreateDecoder;
		scoder.vt->CreateEncoder = SCompressCodecsInfoExternal_CreateEncoder;
		pSetCodecs((ICompressCodecsInfo_*)&scoder);
#endif
	}
	scoder.refs++;
	return S_OK;
}

static int lzmaUnloadExternalCodecs(){
	if(!scoder.refs)return 1;
	pSetCodecs(NULL);
	scoder.vt->Release(&scoder);
	return 0;
}
