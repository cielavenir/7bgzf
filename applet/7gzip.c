#define ZLIBRAWSTDIO_COMPRESS_GZIP
#include "zlibrawstdio_compress.h"

//~~we still use gzip wrapper functions for concatenated gzip files.~~
//now concatenated gzip can be handled on my own.
static int _decompress(FILE *fin,FILE *fout){
	int readlen,headlen;
	int extra_off,extra_len;
	int offset=0;
	int avail_in_orig;

	for(;;){
		readlen=fread(__compbuf+offset,1,COMPBUFLEN-offset,fin)+offset;
		if(!readlen)break;
		headlen=read_gz_header_generic(__compbuf,readlen,&extra_off,&extra_len);
		//for(int i=0;i<16;i++)fprintf(stderr,"%02x",__compbuf[i]);fputc('\n',stderr);
		if(!headlen){
			return 1;
		}
		memmove(__compbuf,__compbuf+headlen,readlen-headlen);
		int status;
#if defined(NOIGZIP)
		z_stream z;
		z.zalloc = Z_NULL;
		z.zfree = Z_NULL;
		z.opaque = Z_NULL;

		if((status=inflateInit2(
			&z, -MAX_WBITS
		)) != Z_OK){
			return status;
		}

		z.next_in = __compbuf;
		z.avail_in = readlen-headlen;
		avail_in_orig = z.avail_in;
		z.next_out = __decompbuf;
		z.avail_out = DECOMPBUFLEN;

		for(;;){
			if(z.avail_in == 0){
				z.next_in = __compbuf;
				z.avail_in = fread(__compbuf,1,COMPBUFLEN,fin);
				avail_in_orig = z.avail_in;
			}
			if(z.avail_out == 0){
				fwrite(__decompbuf,1,DECOMPBUFLEN-z.avail_out,fout);
				z.next_out = __decompbuf;
				z.avail_out = DECOMPBUFLEN;
			}
			status = inflate(&z, Z_NO_FLUSH);
			if(status == Z_STREAM_END)break;
			if(status != Z_OK){
				fprintf(stderr,"inflate: %s\n", (z.msg) ? z.msg : "???");
				return status;
			}
		}
		fwrite(__decompbuf,1,DECOMPBUFLEN-z.avail_out,fout);
		z.avail_in-=8;
		if(z.avail_in<0){
			fread(__decompbuf,1,-z.avail_in,fin);
			z.avail_in=0;
		}
		memmove(__compbuf,__compbuf+avail_in_orig-z.avail_in,z.avail_in);
		offset=z.avail_in;

		status = inflateEnd(&z);
		if(status != Z_OK){
			return status;
		}
#else
		struct inflate_state z;
		isal_inflate_init(&z);
		z.crc_flag = ISAL_GZIP_NO_HDR_VER;

		z.next_in = __compbuf;
		z.avail_in = readlen-headlen;
		avail_in_orig = z.avail_in;
		z.next_out = __decompbuf;
		z.avail_out = DECOMPBUFLEN;

		for(;;){
			if(z.avail_in == 0){
				z.next_in = __compbuf;
				z.avail_in = fread(__compbuf,1,COMPBUFLEN,fin);
				avail_in_orig = z.avail_in;
			}
			if(z.avail_out == 0){
				fwrite(__decompbuf,1,DECOMPBUFLEN-z.avail_out,fout);
				z.next_out = __decompbuf;
				z.avail_out = DECOMPBUFLEN;
			}
			status = isal_inflate(&z);
			if(status != ISAL_DECOMP_OK){
				fprintf(stderr,"isal_inflate: %d\n", status);
				return status;
			}
			if(z.block_state == ISAL_BLOCK_FINISH)break;
		}
		fwrite(__decompbuf,1,DECOMPBUFLEN-z.avail_out,fout);
		/*
		z.avail_in-=0;
		if(z.avail_in<0){
			fread(__decompbuf,1,-z.avail_in,fin);
			z.avail_in=0;
		}
		*/
		memmove(__compbuf,__compbuf+avail_in_orig-z.avail_in,z.avail_in);
		offset=z.avail_in;
#endif
	}

	return 0;
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int _7gzip(const int argc, const char **argv){
#endif
	return zlibrawstdio_main(argc, argv);
}
