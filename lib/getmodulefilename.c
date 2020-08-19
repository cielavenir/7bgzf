#include "../compat.h"
#include "xutil.h" // myfgets
#include <string.h>
#include <dirent.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
#include <link.h>
#endif

#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
#else
int GetModuleFileNameA(void *hModule,char *pFilename,int nSize){
	if(!pFilename)return 0;
	pFilename[0]=0;
#if defined(NODLOPEN)
	return 0;
#elif defined(FEOS)
	char *fname = FeOS_GetModuleName(hModule);
	if(fname){
		int s = strlen(fname);
		int nCopy = min(nSize,s+1);
		memcpy(pFilename,fname,nCopy);
		return nCopy;
	}
#elif defined(__APPLE__)
	// https://stackoverflow.com/a/54201385
	// Since we know the image we want will always be near the end of the list, start there and go backwards
	for(int i=_dyld_image_count()-1; i>=0; i--){
		const char* image_name = _dyld_get_image_name(i);

		// Why dlopen doesn't effect _dyld stuff: if an image is already loaded, it returns the existing handle.
		void* probe_handle = dlopen(image_name, RTLD_LAZY);
		dlclose(probe_handle);

		if(hModule == probe_handle){
			int s = strlen(image_name);
			int nCopy = min(nSize,s+1);
			memcpy(pFilename,image_name,nCopy);
			return nCopy;
		}
	}
//#elif defined(__linux__)
#elif defined(DL_ANDROID)
#if 0
	// workaround to make whole program working.
	// hModule must be function address...
	// [note] keep this code commented for later use, where all proposed method might not work
	Dl_info info;
	dladdr(hModule, &info);
	if(info.dli_fname){
		int s = strlen(info.dli_fname);
		int nCopy = min(nSize,s+1);
		memcpy(pFilename,info.dli_fname,nCopy);
		return nCopy;
	}
#endif
	// be careful that this filename is fully resolved (while dlinfo method resolves only directory name).
	// this can give different filename in dladdr. file sameness should be checked via st_ino, not via filename itself.
	char image_name[768];
	image_name[0]=0;
	DIR *d=opendir("/proc/self/map_files");
	if(d){
		struct dirent *ent;
		while(ent=readdir(d)){
			if(!strcmp(ent->d_name,".")||!strcmp(ent->d_name,".."))continue;
			char x[768];
			sprintf(x,"/proc/self/map_files/%s",ent->d_name);
			size_t siz=readlink(x,image_name,768);
			if(1<=siz && siz<768){
				image_name[siz]=0;
				void* probe_handle = dlopen(image_name, RTLD_LAZY);
				if(probe_handle){
					dlclose(probe_handle);
					if(hModule == probe_handle)break;
				}
			}
			image_name[0]=0;
		}
		closedir(d);
	}
	// it might be possible that opendir succeeds and readdir fails. so using "else" is not ok here.
	if(!image_name[0]){
		FILE *f = fopen("/proc/self/maps","r");
		if(f){
			char line[1024];
			while(myfgets(line,1024,f)){
				int s=strlen(line);
				for(;s>=1&&line[s-1]!=' ';)s--;
				if(strstr(line+s,".so")){
					strcpy(image_name,line+s);
					void* probe_handle = dlopen(image_name, RTLD_LAZY);
					if(probe_handle){
						dlclose(probe_handle);
						if(hModule == probe_handle)break;
					}
					image_name[0]=0;
				}
			}
			fclose(f);
		}
	}
	if(image_name[0]){
		int s = strlen(image_name);
		int nCopy = min(nSize,s+1);
		memcpy(pFilename,image_name,nCopy);
		return nCopy;
	}
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
	struct link_map *lm=NULL;
	dlinfo(hModule, RTLD_DI_LINKMAP, &lm);
	if(lm && lm->l_name){
		int s = strlen(lm->l_name);
		int nCopy = min(nSize,s+1);
		memcpy(pFilename,lm->l_name,nCopy);
		return nCopy;
	}
#endif
	return 0;
}
#endif
