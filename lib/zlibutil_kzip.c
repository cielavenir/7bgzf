// deflation via kzip archive
// this is like on-the-fly version of kzip2gz ( https://encode.su/threads/1630-kzip2gz-a-zip-(single-file-amp-deflate-compression)-to-gz-converter )

#include "../compat.h"
#include "xutil.h"
#include "zlib/zlib.h" // Z_MEM_ERROR, Z_BUF_ERROR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_MMAP 1

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
// in C++, this MMAP/MUNMAP should be handled by subclasses...
#define MMAPR(h, mem, fd, siz) HANDLE *h = CreateFileMapping(_get_osfhandle(fd), NULL, PAGE_READONLY, 0, 0, NULL);char *mem = MapViewOfFile(h, FILE_MAP_READ, 0, 0, siz);
#define MMAPW(h, mem, fd, siz) HANDLE *h = CreateFileMapping(_get_osfhandle(fd), NULL, PAGE_READWRITE, 0, 0, NULL);char *mem = MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, siz);
#define MUNMAP(h, mem, siz) UnmapViewOfFile(mem);CloseHandle(h);
#include <process.h>
// need to self-compile mkstemps on MinGW
int __cdecl mkstemps (char *template_name, int suffix_len);
#else
#include <sys/mman.h>
#define MMAPR(h, mem, fd, siz) char *mem = mmap(NULL, siz, PROT_READ, MAP_PRIVATE, fd, 0);
#define MMAPW(h, mem, fd, siz) char *mem = mmap(NULL, siz, PROT_WRITE, MAP_SHARED, fd, 0);
#define MUNMAP(h, mem, siz) munmap(mem, siz);
#include <sys/wait.h>
#if !defined(__ANDROID__)
#include <spawn.h>
#endif
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

	char *cwd = NULL;
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	// maybe windows should not chdir as current-directory is added to PATH
#else
	cwd = getcwd(NULL, 0);
	if(cwd){
		char *tmpdir=getenv("TMPDIR");
		if(!tmpdir || !*tmpdir){
			struct stat st;
			tmpdir="/tmp";
			if(!stat("/dev/shm",&st) && S_ISDIR(st.st_mode))tmpdir="/dev/shm";
		}
		chdir(tmpdir);
	}
#endif

	char template[128];
	snprintf(template,128,"kzipXXXXXX");
	int fd = mkstemp(template);
#if !USE_MMAP
	write(fd,source,sourceLen);
#else
	{
		ftruncate(fd, sourceLen);
		MMAPW(h, mem, fd, sourceLen)
		if(!mem){
			fprintf(stderr,"failed mmap\n");
			if(cwd){chdir(cwd);free(cwd);}
			return 1;
		}
		memcpy(mem, source, sourceLen);
		MUNMAP(h, mem, sourceLen)
	}
#endif
	close(fd);
	char templatezip[128];
	snprintf(templatezip,128,"kzipXXXXXX.zip");
	int fdzip = mkstemps(templatezip,4);
	/// sad sad sad need to close fdzip because Windows cannot reopen mkstemps-file ///
	close(fdzip);

	char * const args[]={"kzip","-q","-y","-s0","-b256",templatezip,template,NULL};
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	if(_spawnvp(_P_WAIT, "kzip.exe", args)){
		//fprintf(stderr,"aaa %d\n",errno);
		//fprintf(stderr,"aaa %s\n",getenv("PATH"));
		//close(fdzip);
		unlink(template);
		unlink(templatezip);
		if(cwd){chdir(cwd);free(cwd);}
		return -1; // failed to spawn kzip, maybe not found
	}
	unlink(template);
#elif defined(__ANDROID__)
	pid_t pid = fork();
	if(pid==-1){
		//close(fdzip);
		unlink(template);
		unlink(templatezip);
		if(cwd){chdir(cwd);free(cwd);}
		return -1; // failed to fork
	}
	if(pid==0){
		execvp("kzip",args);
		exit(123);
	}
	int status;
	waitpid(pid,&status,WUNTRACED);
	unlink(template);
	if(!WIFEXITED(status)||WEXITSTATUS(status)!=0){
		//close(fdzip);
		unlink(templatezip);
		if(WEXITSTATUS(status)==123){
			if(cwd){chdir(cwd);free(cwd);}
			return -1; // failed to spawn kzip, maybe not found
		}
		if(cwd){chdir(cwd);free(cwd);}
		return Z_MEM_ERROR; // kzip's own failure, better error code?
	}
#else
	char * const envp[]={NULL};
	pid_t pid;
	if(posix_spawnp(&pid,"kzip",NULL,NULL,args,envp)){
		//close(fdzip);
		unlink(template);
		unlink(templatezip);
		if(cwd){chdir(cwd);free(cwd);}
		return -1; // failed to spawn kzip, maybe not found
	}
	int status;
	waitpid(pid,&status,WUNTRACED);
	unlink(template);
	if(!WIFEXITED(status)||WEXITSTATUS(status)!=0){
		//close(fdzip);
		unlink(templatezip);
		if(cwd){chdir(cwd);free(cwd);}
		return Z_MEM_ERROR; // kzip's own failure, better error code?
	}
#endif

	FILE *fzip = fopen(templatezip, "rb");
#if !USE_MMAP
	char headerbuf[256];
	//read(fdzip,headerbuf,256);
	fread(headerbuf,1,256,fzip);
	unsigned int offset,compsize;
	if(read_zip_header(headerbuf,&offset,&compsize,sourceLen) || compsize>*destLen){
		// could not compress well and kzip auto-fallback to store method, but it is not deflate stream
		//close(fdzip);
		fclose(fzip);
		unlink(templatezip);
		if(cwd){chdir(cwd);free(cwd);}
		return store_deflate(dest,destLen,source,sourceLen,level);
	}
	//lseek(fdzip,offset,SEEK_SET);
	//read(fdzip,dest,compsize);
	fseek(fzip,offset,SEEK_SET);
	fread(dest,1,compsize,fzip);
#else
	long long siz = filelengthi64(fileno(fzip));
	MMAPR(h, mem, fileno(fzip), siz)
	if(!mem){
		fprintf(stderr,"failed mmap\n");
		if(cwd){chdir(cwd);free(cwd);}
		return 1;
	}
	unsigned int offset,compsize;
	if(read_zip_header(mem,&offset,&compsize,sourceLen) || compsize>*destLen){
		// could not compress well and kzip auto-fallback to store method, but it is not deflate stream
		//close(fdzip);
		MUNMAP(h, mem, siz)
		fclose(fzip);
		unlink(templatezip);
		if(cwd){chdir(cwd);free(cwd);}
		return store_deflate(dest,destLen,source,sourceLen,level);
	}
	memcpy(dest,mem+offset,compsize);
	MUNMAP(h, mem, siz)
#endif
	*destLen=compsize;
	fclose(fzip);
	unlink(templatezip);
	if(cwd){chdir(cwd);free(cwd);}
	return 0;
#endif
}
