#include "cielbox.h"

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
	//constant phrase
	long long filelengthi64(int fd){
		struct stat st;
		fstat(fd,&st);
		return st.st_size;
	}
	int filelength(int fd){return filelengthi64(fd);}
#endif

long long sfilelengthi64(const char *path){
	struct stat st;
	stat(path,&st);
	return st.st_size;
}
int sfilelength(const char *path){return sfilelengthi64(path);}

int filemode(int fd){
	struct stat st;
	fstat(fd,&st);
	return st.st_mode;
}

int sfilemode(const char *path){
	struct stat st;
	stat(path,&st);
	return st.st_mode;
}

unsigned char buf[BUFLEN];
unsigned char __compbuf[COMPBUFLEN],__decompbuf[DECOMPBUFLEN];

/* main like busybox */

#define TRIVIAL_INDEX 5
#define USUAL_INDEX 5
static const applet apps[]={ //should be sorted
	{"applets",applets},
	{"list",applets},
	{"--list",applets},
	{"install",_install},
	{"--install",_install},

//usual list
	{"7bgzf",_7bgzf},
	{"7ciso",_7ciso},
	{"7daxcr",_7daxcr},
	{"7dictzip",_7dictzip},
	{"7gzinga",_7gzinga},
	{"7gzip",_7gzip},
	{"7migz",_7migz},
	{"7png",_7png},
	{"7razf",_7razf},
	{"zlibrawstdio",zlibrawstdio},
	{"zlibrawstdio2",zlibrawstdio2},
};
static const int appsize=sizeof(apps)/sizeof(applet);

int applets(const int argc, const char **argv){
	int i=TRIVIAL_INDEX;
	for(;i<appsize;i++){fprintf(stdout,"%s\n",apps[i].name);} //if(i<appsize-1)fputc(',',stdout);}
	return 0;
}

int _link(const int argc, const char **argv){
	if(argc<2){fprintf(stderr,"tell me cielbox path\n");return 1;}
	int i=USUAL_INDEX;
	if(!strcmp(argv[0],"link-full")||!strcmp(argv[0],"--link-full"))i=TRIVIAL_INDEX;
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	//int f=1;
	typedef int (__stdcall*CreateHardLinkA_type)(const char*, const char*, void*);
	CreateHardLinkA_type pCreateHardLinkA=(CreateHardLinkA_type)GetProcAddress(GetModuleHandleA("kernel32"),"CreateHardLinkA");
	if(!pCreateHardLinkA)fprintf(stderr,"link isn't supported, fallback to copy...\n");
	for(;i<appsize;i++){
		sprintf(cbuf,"%s.exe",apps[i].name);
		if(pCreateHardLinkA){
			int ret;
			ret=DeleteFileA(cbuf);
			//if(!ret)fprintf(stderr,"delete %s failed\n",cbuf);
			ret=pCreateHardLinkA(cbuf,argv[1],NULL);
			if(!ret){
				fprintf(stderr,
					"linking failed, fallback to copy...\n"
					"if NTFS I hope copying also fails\n"
				);
				pCreateHardLinkA=NULL;goto copy;
			}
		}else{
copy:
			CopyFileA(argv[1],cbuf,0);
		}
	}
#else
	for(;i<appsize;i++){
		/*
		sprintf(cbuf,"ln -sf %s %s",argv[1],apps[i].name);
		system(cbuf);
		*/
		struct stat st;
		if((!lstat(apps[i].name,&st)&&unlink(apps[i].name))||symlink(argv[1],apps[i].name)){
			fprintf(stderr,"install failed: %s\n",apps[i].name);
		}
	}
#endif
	return 0;
}

static const char *_linkarg[]={
	"--link-full",
	"",
};
int _install(const int argc, const char **argv){
	const char *exe=argv[-1]; //bah!
	if(access(exe,0)){
		fprintf(stderr,
			"Fatal: cannot detect cielbox's presense.\n"
			"Perhaps you invoked cielbox from PATH.\n"
			"Please invoke cielbox using filepath.\n"
		);
		return 1;
	}
	_linkarg[1]=exe;
	return _link(2,_linkarg);
}

__attribute__((noreturn)) static void usage(){
	const char *arch=
#if defined(__x86_64__)
	#if defined(__ILP32__)
		"x32"
	#else
		"amd64"
	#endif
#elif defined(__i386__)
	"i386"
#elif defined(__ia64__)
	"ia64"
#elif defined(__ppc64__)
	"ppc64"
#elif defined(__ppc__)
	"ppc"
#elif defined(__aarch64__)
	"arm64"
#elif defined(__arm__)
	"arm"
#elif defined(__mips__)
	"mips"
#elif defined(__m68k__)
	"m68k"
#elif defined(__sparc__)
	"sparc"
#elif defined(__alpha__)
	"alpha"
#elif defined(__sh__)
	"sh"
#else
	"other"
#endif
	;

	const char *compiler=
#if defined(__INTEL_COMPILER)
	" [icc]"
#elif defined(__OPENCC__)
	" [opencc]"
#elif defined(__BORLANDC__)
	" [bcc]"
#elif defined(_MSC_VER)
	" [msvc]"
#elif defined(__clang__)
	" [clang]"
#elif defined(__llvm__)
	" [llvm-gcc]"
#elif defined(__GNUC__)
	" [gcc]"
#else
	""
#endif
	;

	int bits=sizeof(void*)*CHAR_BIT;
	fprintf(stderr,
		"CielBox multi-call binary [7bgzf edition] (%s %dbit%s)\n"
		"Revision %d (Built on "__DATE__")\n"
		"cielbox [applet] [arg]\n"
		"cielbox --list: show list of applets\n"
		"cielbox --link[-full] /path/to/cielbox\n"
		"/path/to/cielbox --install: make applet link to cielbox\n"
		"cielbox --license: show license notice\n"
	,arch,bits,compiler,BOX_REVISION);

	exit(-1);//while(1);
}

int main(const int argc, const char **argv){
	int f=0,i=0;
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
	char sep='\\';
#else
	char sep='/';
#endif
	char *exe;
	initstdio();
#if defined(FEOS)
	xor_srand((unsigned int)time(NULL));
#else
	xor_srand((unsigned int)time(NULL)^(getpid()<<16));
#endif

	for(i=strlen(argv[0])-1;i>=0;i--){
		if(argv[0][i]==sep)break;
		if(!f&&argv[0][i]=='.')*(char*)(argv[0]+i)=0,f=1;
	}
	exe=(char*)argv[0]+i+1;
	for(i=TRIVIAL_INDEX;i<appsize;i++)if(!strcasecmp(exe,apps[i].name))return apps[i].func(argc,argv);
	if(argc<2)usage();
	for(i=0;i<appsize;i++)if(!strcasecmp(argv[1],apps[i].name))return apps[i].func(argc-1,argv+1);
	usage();
}
