#define ZLIBRAWSTDIO_COMPRESS_ZLIB
#include "zlibrawstdio_compress.h"

static int _decompress(FILE *fin,FILE *fout){
	int status;
#if defined(NOIGZIP)
	z_stream z;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	if(inflateInit(&z) != Z_OK){
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
	z.crc_flag = ISAL_ZLIB;

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
int zlibrawstdio(const int argc, const char **argv){
#endif
	return zlibrawstdio_main(argc, argv);
}
