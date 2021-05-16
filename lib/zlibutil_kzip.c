// deflation via kzip archive
// this is like on-the-fly version of kzip2gz ( https://encode.su/threads/1630-kzip2gz-a-zip-(single-file-amp-deflate-compression)-to-gz-converter )

#include "../compat.h"
#include "xutil.h"
#include "zlib/zlib.h" // Z_MEM_ERROR, Z_BUF_ERROR
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#include <process.h>
// need to self-compile mkstemps on MinGW
int __cdecl mkstemps (char *template_name, int suffix_len);
#else
#include <sys/wait.h>
#include <spawn.h>
#endif

static int read_zip_header(char *headerbuf,unsigned int *offset,unsigned int *compsize,unsigned int decompsize){
	if(read32(headerbuf)!=0x04034b50)return 1;
	if(read16(headerbuf+8)!=8)return 1;
	if(read32(headerbuf+22)!=decompsize)return 1;
	*compsize=read32(headerbuf+18);
	*offset=30+read16(headerbuf+26)+read16(headerbuf+28);
	return 0;
}

int store_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
);

int kzip_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
#ifdef FEOS
	return -1;
#else
	//char *tmpdir=getenv("TMPDIR");
	//if(!tmpdir || !*tmpdir)tmpdir="/tmp";

	char template[128];
	snprintf(template,128,"kzipXXXXXX");
	int fd = mkstemp(template);
	write(fd,source,sourceLen);
	close(fd);
	char templatezip[128];
	snprintf(templatezip,128,"kzipXXXXXX.zip");
	int fdzip = mkstemps(templatezip,4);
	/// sad sad sad need to close fdzip because Windows cannot reopen mkstemps-file ///
	close(fdzip);

	char * const args[]={"kzip","-q","-y","-s0","-b256",templatezip,template,NULL};
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	if(_spawnvp(_P_WAIT, "kzip.exe", args)){
		fprintf(stderr,"aaa %d\n",errno);
		fprintf(stderr,"aaa %s\n",getenv("PATH"));
		//close(fdzip);
		unlink(template);
		unlink(templatezip);
		return -1; // failed to spawn kzip, maybe not found
	}
	unlink(template);
#else
	char * const envp[]={NULL};
	pid_t pid;
	if(posix_spawnp(&pid,"kzip",NULL,NULL,args,envp)){
		//close(fdzip);
		unlink(template);
		unlink(templatezip);
		return -1; // failed to spawn kzip, maybe not found
	}
	int status;
	waitpid(pid,&status,WUNTRACED);
	unlink(template);
	if(!WIFEXITED(status)||WEXITSTATUS(status)!=0){
		//close(fdzip);
		unlink(templatezip);
		return Z_MEM_ERROR; // kzip's own failure, better error code?
	}
#endif

	FILE *fzip = fopen(templatezip, "rb");
	char headerbuf[256];
	//read(fdzip,headerbuf,256);
	fread(headerbuf,1,256,fzip);
	unsigned int offset,compsize;
	if(read_zip_header(headerbuf,&offset,&compsize,sourceLen) || compsize>*destLen){
		// could not compress well and kzip auto-fallback to store method, but it is not deflate stream
		//close(fdzip);
		fclose(fzip);
		unlink(templatezip);
		return store_deflate(dest,destLen,source,sourceLen,level);
	}
	//lseek(fdzip,offset,SEEK_SET);
	//read(fdzip,dest,compsize);
	fseek(fzip,offset,SEEK_SET);
	fread(dest,1,compsize,fzip);
	*destLen=compsize;
	fclose(fzip);
	unlink(templatezip);
	return 0;
#endif
}
