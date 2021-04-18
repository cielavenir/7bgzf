#define ZLIBRAWSTDIO_COMPRESS_DEFLATE
#include "zlibrawstdio_compress.h"

static int _decompress(FILE *fin,FILE *fout){
#if 0
	if(!lzmaOpen7z()){
		void *coder=NULL;
		lzmaCreateCoder(&coder,0x040108,0,0);
		if(coder){
			lzmaCodeCallback(coder,fin,fread2,fin==stdin ? NULL : (tClose)fclose,fout,fwrite2,fout==stdout ? NULL : (tClose)fclose);
			lzmaDestroyCoder(&coder);
		}
		lzmaClose7z();
		return 0;
	}
#endif

	int status;
#if defined(NOIGZIP)
	z_stream z;

	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if(inflateInit2(&z,-MAX_WBITS) != Z_OK){
		fprintf(stderr,"inflateInit: %s\n", (z.msg) ? z.msg : "???");
		return 1;
	}

	z.next_in = __compbuf;
	z.avail_in = fread(__compbuf,1,COMPBUFLEN,fin);
	z.next_out = __decompbuf;
	z.avail_out = DECOMPBUFLEN;

	for(;;){
		status = inflate(&z, Z_NO_FLUSH);
		if(status == Z_STREAM_END)break;
		if(status != Z_OK){
			fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
			return 10;
		}

		//goto next buffer
		if(z.avail_in == 0){
			z.next_in = __compbuf;
			z.avail_in = fread(__compbuf,1,COMPBUFLEN,fin);
			if(z.avail_in == 0){fprintf(stderr,"deflated data truncated.\n");return 11;}
		}
		if(z.avail_out == 0){
			fwrite(__decompbuf,1,DECOMPBUFLEN,fout);
			z.next_out = __decompbuf;
			z.avail_out = DECOMPBUFLEN;
		}
	}
	fwrite(__decompbuf,1,DECOMPBUFLEN-z.avail_out,fout);

	if(inflateEnd(&z) != Z_OK){
		fprintf(stderr,"inflateEnd: %s\n", (z.msg) ? z.msg : "???");
		return 2;
	}
#else
	struct inflate_state z;
	isal_inflate_init(&z);
	z.crc_flag = 0;

	z.next_in = __compbuf;
	z.avail_in = fread(__compbuf,1,COMPBUFLEN,fin);
	z.next_out = __decompbuf;
	z.avail_out = DECOMPBUFLEN;

	for(;;){
		status = isal_inflate(&z);
		if(status != Z_OK){
			fprintf(stderr,"isal_inflate: %d\n", status);
			return 10;
		}
		if(z.block_state == ISAL_BLOCK_FINISH)break;

		//goto next buffer
		if(z.avail_in == 0){
			z.next_in = __compbuf;
			z.avail_in = fread(__compbuf,1,COMPBUFLEN,fin);
			if(z.avail_in == 0){fprintf(stderr,"deflated data truncated.\n");return 11;}
		}
		if(z.avail_out == 0){
			fwrite(__decompbuf,1,DECOMPBUFLEN,fout);
			z.next_out = __decompbuf;
			z.avail_out = DECOMPBUFLEN;
		}
	}
	fwrite(__decompbuf,1,DECOMPBUFLEN-z.avail_out,fout);
#endif
	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int zlibrawstdio2(const int argc, const char **argv){
#endif
	return zlibrawstdio_main(argc, argv);
}
