#include "types.h"

#if 1 // defined(TARGET_PS4) || BUILD_OS==OS_PS4_BSD


#include "ps4.h"


#include <kernel_ex.h>
#include <sysmodule_ex.h>
#include <system_service_ex.h>
#include <shellcore_util.h>
#include <piglet.h>
#include <pad.h>
#include <audioout.h>

#include <kernel.h>
#include <video_out.h>
#include <user_service.h>
#include <system_service.h>
#include <gnm.h>


#define EPRINTF printf


static SceKernelModule s_piglet_module = -1;
static SceKernelModule s_shcomp_module = -1;

const char* g_sandbox_word = NULL;


static bool load_modules(void)
{
	int ret;

	ret = sceKernelLoadStartModule(MODULE_PATH_PREFIX "/" PIGLET_MODULE_NAME, 0, NULL, 0, NULL, NULL);
	if (ret < 0) {
		EPRINTF("sceKernelLoadStartModule(%s) failed: 0x%08X\n", PIGLET_MODULE_NAME, ret);
		goto err;
	}
	s_piglet_module = ret;

	ret = sceKernelLoadStartModule(MODULE_PATH_PREFIX "/" SHCOMP_MODULE_NAME, 0, NULL, 0, NULL, NULL);
	if (ret < 0) {
		EPRINTF("sceKernelLoadStartModule(%s) failed: 0x%08X\n", SHCOMP_MODULE_NAME, ret);
		goto err;
	}
	s_shcomp_module = ret;

	return true;

err_unload_shcompt:
	ret = sceKernelStopUnloadModule(s_shcomp_module, 0, NULL, 0, NULL, NULL);
	if (ret < 0) {
		EPRINTF("sceKernelStopUnloadModule(%s) failed: 0x%08X\n", SHCOMP_MODULE_NAME, ret);
	}
	else {
		s_shcomp_module = -1;
	}

err_unload_piglet:
	ret = sceKernelStopUnloadModule(s_piglet_module, 0, NULL, 0, NULL, NULL);
	if (ret < 0) {
		EPRINTF("sceKernelStopUnloadModule(%s) failed: 0x%08X\n", PIGLET_MODULE_NAME, ret);
	}
	else {
		s_piglet_module = -1;
	}

err:
	return false;
}

static void unload_modules(void)
{
	int ret;

	if (s_shcomp_module > 0) {
		ret = sceKernelStopUnloadModule(s_shcomp_module, 0, NULL, 0, NULL, NULL);
		if (ret < 0) {
			EPRINTF("sceKernelStopUnloadModule(%s) failed: 0x%08X\n", SHCOMP_MODULE_NAME, ret);
		}
		else {
			s_shcomp_module = -1;
		}
	}

	if (s_piglet_module > 0) {
		ret = sceKernelStopUnloadModule(s_piglet_module, 0, NULL, 0, NULL, NULL);
		if (ret < 0) {
			EPRINTF("sceKernelStopUnloadModule(%s) failed: 0x%08X\n", PIGLET_MODULE_NAME, ret);
		}
		else {
			s_piglet_module = -1;
		}
	}
}

/* XXX: patches below are given for Piglet module from 4.74 Devkit PUP */
static void pgl_patches_cb(void* arg, uint8_t* base, uint64_t size)
{
	/* Patch runtime compiler check */
	const uint8_t p_set_eax_to_1[] = {
		0x31, 0xC0, 0xFF, 0xC0, 0x90,
	};
	memcpy(base + 0x5451F, p_set_eax_to_1, sizeof(p_set_eax_to_1));

	/* Tell that runtime compiler exists */
	*(uint8_t*)(base + 0xB2DEC) = 0;
	*(uint8_t*)(base + 0xB2DED) = 0;
	*(uint8_t*)(base + 0xB2DEE) = 1;
	*(uint8_t*)(base + 0xB2E21) = 1;

	/* Inform Piglet that we have shader compiler module loaded */
	*(int32_t*)(base + 0xB2E24) = s_shcomp_module;
}


/// util.h

SceKernelModule load_module_from_sandbox(const char* name, size_t args, const void* argp, unsigned int flags, const SceKernelLoadModuleOpt* opts, int* res);

bool get_module_base(const char* name, uint64_t* base, uint64_t* size);

typedef void module_patch_cb_t(void* arg, uint8_t* base, uint64_t size);
bool patch_module(const char* name, module_patch_cb_t* cb, void* arg);

void hexdump(const void* data, size_t size);

void send_notify(const char* format, ...);

//// end util.h


static bool do_patches(void) {
	if (!patch_module(PIGLET_MODULE_NAME, &pgl_patches_cb, NULL)) {
		EPRINTF("Unable to patch PGL module.\n");
		goto err;
	}

	return true;

err:
	return false;
}

static void cleanup(void) {
	int ret;

	unload_modules();

	ret = sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
	if (ret) {
		EPRINTF("sceSysmoduleUnloadModuleInternal(%s) failed: 0x%08X\n", "SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE", ret);
	}
}


#define SM_AudioOut			0x80000001
#define SM_SystemService	0x80000010
#define SM_UserService		0x80000011
#define SM_Pad				0x80000024
#define SM_SysUtil			0x80000026

#define SM_ContentExport	SCE_SYSMODULE_CONTENT_EXPORT


SceUserServiceUserId userId = SCE_USER_SERVICE_USER_ID_SYSTEM;


void ps4_module_init()
{
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_AudioOut));
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_SystemService));
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_UserService));
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_Pad));
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_SysUtil));

	verify(SCE_OK == sceUserServiceInitialize(NULL));
	sceUserServiceGetInitialUser(&userId);


	g_sandbox_word = sceKernelGetFsSandboxRandomWord();
	if (!g_sandbox_word) {
		EPRINTF("sceKernelGetFsSandboxRandomWord failed.\n");
		return;
	}

	printf("load_modules();\n");

	verify(load_modules());
	verify(do_patches());
	verify(SCE_OK == sceSystemServiceHideSplashScreen());

}





//////// util.c



SceKernelModule load_module_from_sandbox(const char* name, size_t args, const void* argp, unsigned int flags, const SceKernelLoadModuleOpt* opts, int* res)
{
	char file_path[SCE_KERNEL_MAX_NAME_LENGTH];
	SceKernelModule module;

	snprintf(file_path, sizeof(file_path), "/%s/common/lib/%s", g_sandbox_word, name);

	if(1 > (module = sceKernelLoadStartModule(file_path, args, argp, flags, opts, res)))
		printf("Error, sceKernelLoadStartModule() failed with 0x%08X\n", (unat)module);

	return module;
}

bool get_module_base(const char* name, uint64_t* base, uint64_t* size) {
	SceKernelModuleInfo moduleInfo;
	int ret;

	ret = sceKernelGetModuleInfoByName(name, &moduleInfo);
	if (ret) {
		EPRINTF("sceKernelGetModuleInfoByName(%s) failed: 0x%08X\n", name, ret);
		goto err;
	}

	if (base) {
		*base = (uint64_t)moduleInfo.segmentInfo[0].baseAddr;
	}
	if (size) {
		*size = moduleInfo.segmentInfo[0].size;
	}

	return true;

err:
	return false;
}

bool patch_module(const char* name, module_patch_cb_t* cb, void* arg) {
	uint64_t base, size;
	int ret;

	if (!get_module_base(name, &base, &size)) {
		goto err;
	}

	ret = sceKernelMprotect((void*)base, size, SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_CPU_EXEC);
	if (ret) {
		EPRINTF("sceKernelMprotect(%s) failed: 0x%08X\n", name, ret);
		goto err;
	}

	if (cb) {
		(*cb)(arg, (uint8_t*)base, size);
	}

	return true;

err:
	return false;
}

void send_notify(const char* format, ...) {
	SceNotificationRequest req;
	va_list args;

	memset(&req, 0, sizeof(req));
	{
		req.unk_0x10 = -1;

		va_start(args, format);
		req.buf[0] = '%'; /* XXX: hackity hack */
		vsnprintf((char*)req.buf + 1, 0xB4, format, args);
		va_end(args);
	}

	sceKernelSendNotificationRequest(0, &req, sizeof(req), 1);
}




extern "C" int umask(int newmask) {
	return syscall(60, newmask);
}
	
//sysent[482] 0xffffffff` 9b0e58d0 "shm_open"
extern "C" int shm_open(const char *path,	int flags, mode_t mode)
{
	return syscall(482, path, flags, mode);
}

//sysent[483] 0xffffffff` 9b0e61c0 "shm_unlink"
extern "C" int shm_unlink(const char *path)
{
	return syscall(483, path);
}


typedef struct _dirdesc {
	int	dd_fd;		/* file descriptor associated with directory */
	long	dd_loc;		/* offset in current buffer */
	long	dd_size;	/* amount of data returned by getdirentries */
	char	*dd_buf;	/* data buffer */
	int	dd_len;		/* size of data buffer */
	long	dd_seek;	/* magic cookie returned by getdirentries */
	long	dd_rewind;	/* magic cookie for rewinding */
	int	dd_flags;	/* flags for readdir */
	struct pthread_mutex	*dd_lock;	/* lock */
	struct _telldir *dd_td;	/* telldir position recording */
} DIR;








DIR * opendir (const char *name)
{
	register DIR *dirp;
	register int fd;
	int rc = 0;

	//if ((fd = sceKernelOpen(name, SCE_KERNEL_O_RDONLY|SCE_KERNEL_O_DIRECTORY, 0)) == -1)
	if ((fd = open(name, O_RDONLY | O_DIRECTORY, 0)) == -1)
		return NULL;

	//rc = sceKernelFcntl(fd, F_GETFL, 1);
	rc = fcntl(fd, F_GETFL, 1);

	if (rc == -1 ||
	    (dirp = (DIR *)malloc(sizeof(DIR))) == NULL) {
		//sceKernelClose(fd);
		close(fd);
		return NULL;
	}

	dirp->dd_buf = (char*)malloc (1024);
	dirp->dd_len = 1024;

	if (dirp->dd_buf == NULL) {
		free (dirp);
		//sceKernelClose(fd);
		close(fd);
		return NULL;
	}
	dirp->dd_fd = fd;
	dirp->dd_loc = 0;
	dirp->dd_seek = 0;

	return dirp;
}

int closedir (register DIR *dirp)
{
	int rc;

	//rc = sceKernelClose(dirp->dd_fd);
	rc = close(dirp->dd_fd);
	free((void *)dirp->dd_buf);
	free((void *)dirp);
	return rc;
}

struct dirent * readdir (register DIR *dirp)
{
  register struct dirent *dp;
 
  for (;;) {
    if (dirp->dd_loc == 0) {

    //dirp->dd_size = sceKernelGetdents(dirp->dd_fd,
    dirp->dd_size = getdents(dirp->dd_fd,
				dirp->dd_buf,
				dirp->dd_len);

      if (dirp->dd_size <= 0) {
	return NULL;
      }
    }
    if (dirp->dd_loc >= dirp->dd_size) {
      dirp->dd_loc = 0;
      continue;
    }
    dp = (struct dirent *)(dirp->dd_buf + dirp->dd_loc);

    if (dp->d_reclen <= 0 ||
	dp->d_reclen > dirp->dd_len + 1 - dirp->dd_loc) {
      return NULL;
    }
    dirp->dd_loc += dp->d_reclen;
    if (dp->d_fileno == 0)
      continue;
    return (dp);
  }
}













#endif //TARGET_PS4