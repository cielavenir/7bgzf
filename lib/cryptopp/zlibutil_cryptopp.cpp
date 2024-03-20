#include <stdio.h>

#include "../zlib/zlib.h"
#include <cryptopp/zdeflate.h>
#include <cryptopp/zinflate.h>

extern "C" int cryptopp_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
	CryptoPP::Deflator zipper(NULL,level,CryptoPP::Deflator::DEFAULT_LOG2_WINDOW_SIZE,false);
	//zipper.Put(source,sourceLen);
	zipper.Put2(source,sourceLen,1,true);
	//zipper.MessageEnd(); // Put2 already sends MessageEnd, so this line will cause double-call (?)
	size_t avail = zipper.MaxRetrievable();
	if(!avail)return Z_NEED_DICT; // ???
	if(avail>*destLen)return Z_BUF_ERROR;
	*destLen = avail;
	zipper.Get(dest,avail);
	return 0;
}

extern "C" int cryptopp_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
){
	CryptoPP::Inflator zipper;
	try{
		//zipper.Put(source,sourceLen);
		zipper.Put2(source,sourceLen,1,true);
		//zipper.MessageEnd();
	}catch(const CryptoPP::Inflator::BadBlockErr&){
		return Z_DATA_ERROR;
	}catch(const CryptoPP::Inflator::BadDistanceErr&){
		return Z_DATA_ERROR;
	}
	size_t avail = zipper.MaxRetrievable();
	if(avail>*destLen)return Z_BUF_ERROR;
	*destLen = avail;
	zipper.Get(dest,avail);
	return 0;
}
