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


#if 1 //ndef PS4_PROVIDE_DIR


	DIR           *opendir(const char *) { return nullptr; }
	int           closedir(DIR *) {return 0;}
	struct dirent *readdir(DIR *) {return nullptr;}

	int mkstemp(char *tmplate) { return 0; }

	extern "C" int umask(int newmask) {
		return syscall(60, newmask);
	}

#else //#if 0 //PS4_PROVIDE_DIR




#include <sys/queue.h>

/*
 * One of these structures is malloced to describe the current directory
 * position each time telldir is called. It records the current magic
 * cookie returned by getdirentries and the offset within the buffer
 * associated with that return value.
 */
struct ddloc {
	LIST_ENTRY(ddloc) loc_lqe; /* entry in list */
	long	loc_index;	/* key associated with structure */
	long	loc_seek;	/* magic cookie returned by getdirentries */
	long	loc_loc;	/* offset of entry in buffer */
};

/*
 * One of these structures is malloced for each DIR to record telldir
 * positions.
 */
struct _telldir {
	LIST_HEAD(, ddloc) td_locq; /* list of locations */
	long	td_loccnt;	/* index of entry for sequential readdir's */
};




/*
 * Structure describing an open directory.
 *
 * NOTE. Change structure layout with care, at least dd_fd field has to
 * remain unchanged to guarantee backward compatibility.
 */
struct _dirdesc {
	int		dd_fd;			/* file descriptor associated with directory */
	long	dd_loc;			/* offset in current buffer */
	long	dd_size;		/* amount of data returned by getdirentries */
	char	*dd_buf;		/* data buffer */
	int		dd_len;			/* size of data buffer */
	off_t	dd_seek;		/* magic cookie returned by getdirentries */
	long	dd_rewind;		/* magic cookie for rewinding */
	int		dd_flags;		/* flags for readdir */
	struct pthread_mutex *dd_lock;	/* lock */
	struct _telldir *dd_td;	/* telldir position recording */
	void	*dd_compat_de;	/* compat dirent */
};

#if 0 // found 'elsewhere'
typedef struct _dirdesc {
	int dd_fd; /* file descriptor associated with directory */
	long dd_loc; /* offset in current buffer */
	long dd_size; /* amount of data returned bygetdirentries */
	char *dd_buf; /* data buffer */
	int dd_len; /* size of data buffer */
	long dd_seek; /* magic cookie returned by getdirentries */
	long dd_rewind; /* magic cookie for rewinding */
	int dd_flags; /* flags for readdir */
} DIR;
#endif

#define	_dirfd(dirp)	((dirp)->dd_fd)





#define __isthreaded 1
#define _pthread_mutex_lock pthread_mutex_lock
#define _pthread_mutex_unlock pthread_mutex_unlock

#define _pthread_mutex_destroy pthread_mutex_destroy


//////////////////////////////////// getdirentries
//getdirentries(int fd, char *buf, u_int count, long *basep)

#if 1
#define _getdirentries getdirentries
#else

ssize_t _getdirentries(int fd, char *buf, size_t nbytes, off_t *basep)
{
	char *oldbuf;
	size_t len;
	ssize_t rv;

	if (__getosreldate() >= INO64_FIRST)
		return (__sys_getdirentries(fd, buf, nbytes, basep));

	/*
	 * Because the old system call returns entries that are smaller than the
	 * new, we could wind up in a situation where we have too many to fit in
	 * the buffer with the new encoding. So sacrifice a small bit of
	 * efficiency to ensure that never happens. We pick 1/4 the size round
	 * up to the next DIRBLKSIZ. This will guarnatee enough room exists in
	 * the dst buffer due to changes in efficiency in packing dirent
	 * entries. We don't check against minimum block size to avoid a lot of
	 * stat calls, we'll see if that's wise or not.
	 * TBD: Will this difference matter to lseek?
	 */
	len = roundup(nbytes / 4, DIRBLKSIZ);
	oldbuf = malloc(len);
	if (oldbuf == NULL) {
		errno = EINVAL;		/* ENOMEM not in possible list */
		return (-1);
	}
	rv = getdirentries(fd, oldbuf, len, basep);
	if (rv == -1) {
		free(oldbuf);
		return (rv);
	}
	if (rv > 0)
		rv = __cvt_dirents_from11(oldbuf, rv, buf, nbytes);
	free(oldbuf);

	return (rv);
}

#endif

/////////////////// freebsd readdir.c /////////////////// 


/*
 * get next entry in a directory.
 */
struct dirent *
_readdir_unlocked(DIR *dirp, int skip)
{
	struct dirent *dp;

	for (;;) {
		if (dirp->dd_loc >= dirp->dd_size) {
			if (dirp->dd_flags & __DTF_READALL)
				return (NULL);
			dirp->dd_loc = 0;
		}
		if (dirp->dd_loc == 0 && !(dirp->dd_flags & __DTF_READALL)) {
			dirp->dd_size = _getdirentries(dirp->dd_fd, dirp->dd_buf, dirp->dd_len, &dirp->dd_seek);
			if (dirp->dd_size <= 0)
				return (NULL);
		}
		dp = (struct dirent *)(dirp->dd_buf + dirp->dd_loc);
		if ((long)dp & 03L)	/* bogus pointer check */
			return (NULL);
		if (dp->d_reclen <= 0 ||
		    dp->d_reclen > dirp->dd_len + 1 - dirp->dd_loc)
			return (NULL);
		dirp->dd_loc += dp->d_reclen;
	//	if (dp->d_ino == 0 && skip)
	//		continue;
		if (dp->d_type == DT_WHT && (dirp->dd_flags & DTF_HIDEW))
			continue;
		return (dp);
	}
}

struct dirent *
readdir(DIR *dirp)
{
	struct dirent	*dp;

	if (__isthreaded) {
		_pthread_mutex_lock(&dirp->dd_lock);
		dp = _readdir_unlocked(dirp, 1);
		_pthread_mutex_unlock(&dirp->dd_lock);
	}
	else
		dp = _readdir_unlocked(dirp, 1);
	return (dp);
}

int
readdir_r(DIR *dirp,
	struct dirent *entry,
	struct dirent **result)
{
	struct dirent *dp;
	int saved_errno;

	saved_errno = errno;
	errno = 0;
	if (__isthreaded) {
		_pthread_mutex_lock(&dirp->dd_lock);
		if ((dp = _readdir_unlocked(dirp, 1)) != NULL)
			memcpy(entry, dp, _GENERIC_DIRSIZ(dp));
		_pthread_mutex_unlock(&dirp->dd_lock);
	}
	else if ((dp = _readdir_unlocked(dirp, 1)) != NULL)
		memcpy(entry, dp, _GENERIC_DIRSIZ(dp));

	if (errno != 0) {
		if (dp == NULL)
			return (errno);
	} else
		errno = saved_errno;

	if (dp != NULL)
		*result = entry;
	else
		*result = NULL;

	return (0);
}



/////////////////// freebsd telldir.c /////////////////// 



/*
 * Reclaim memory for telldir cookies which weren't used.
 */
void
_reclaim_telldir(DIR *dirp)
{
	struct ddloc *lp;
	struct ddloc *templp;

	lp = LIST_FIRST(&dirp->dd_td->td_locq);
	while (lp != NULL) {
		templp = lp;
		lp = LIST_NEXT(lp, loc_lqe);
		free(templp);
	}
	LIST_INIT(&dirp->dd_td->td_locq);
}




/////////////////// freebsd closedir.c /////////////////// 




/*
 * close a directory.
 */
int
closedir(DIR *dirp)
{
	int fd;

	if (__isthreaded)
		_pthread_mutex_lock(&dirp->dd_lock);
	_seekdir(dirp, dirp->dd_rewind);	/* free seekdir storage */
	fd = dirp->dd_fd;
	dirp->dd_fd = -1;
	dirp->dd_loc = 0;
	free((void *)dirp->dd_buf);
	_reclaim_telldir(dirp);
	if (__isthreaded) {
		_pthread_mutex_unlock(&dirp->dd_lock);
		_pthread_mutex_destroy(&dirp->dd_lock);
	}
	free((void *)dirp);
	return(_close(fd));
}




/////////////////// freebsd opendir.c ///////////////////



static DIR * __opendir_common(int, const char *, int);

/*
 * Open a directory.
 */
DIR *
opendir(const char *name)
{

	return (__opendir2(name, DTF_HIDEW|DTF_NODUP));
}

/*
 * Open a directory with existing file descriptor.
 */
DIR *
fdopendir(int fd)
{
	struct stat statb;

	/* Check that fd is associated with a directory. */
	if (_fstat(fd, &statb) != 0)
		return (NULL);
	if (!S_ISDIR(statb.st_mode)) {
		errno = ENOTDIR;
		return (NULL);
	}
	if (_fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		return (NULL);
	return (__opendir_common(fd, NULL, DTF_HIDEW|DTF_NODUP));
}

DIR *
__opendir2(const char *name, int flags)
{
	int fd;

	if ((fd = _open(name,
	    O_RDONLY | O_NONBLOCK | O_DIRECTORY | O_CLOEXEC)) == -1)
		return (NULL);

	return __opendir_common(fd, name, flags);
}

static int
opendir_compar(const void *p1, const void *p2)
{

	return (strcmp((*(const struct dirent **)p1)->d_name,
	    (*(const struct dirent **)p2)->d_name));
}

/*
 * Common routine for opendir(3), __opendir2(3) and fdopendir(3).
 */
static DIR *
__opendir_common(int fd, const char *name, int flags)
{
	DIR *dirp;
	int incr;
	int saved_errno;
	int unionstack;

	if ((dirp = malloc(sizeof(DIR) + sizeof(struct _telldir))) == NULL)
		return (NULL);

	dirp->dd_td = (struct _telldir *)((char *)dirp + sizeof(DIR));
	LIST_INIT(&dirp->dd_td->td_locq);
	dirp->dd_td->td_loccnt = 0;

	/*
	 * Use the system page size if that is a multiple of DIRBLKSIZ.
	 * Hopefully this can be a big win someday by allowing page
	 * trades to user space to be done by _getdirentries().
	 */
	incr = getpagesize();
	if ((incr % DIRBLKSIZ) != 0) 
		incr = DIRBLKSIZ;

	/*
	 * Determine whether this directory is the top of a union stack.
	 */
	if (flags & DTF_NODUP) {
		struct statfs sfb;

		if (_fstatfs(fd, &sfb) < 0)
			goto fail;
		unionstack = !strcmp(sfb.f_fstypename, "unionfs")
		    || (sfb.f_flags & MNT_UNION);
	} else {
		unionstack = 0;
	}

	if (unionstack) {
		int len = 0;
		int space = 0;
		char *buf = 0;
		char *ddptr = 0;
		char *ddeptr;
		int n;
		struct dirent **dpv;

		/*
		 * The strategy here is to read all the directory
		 * entries into a buffer, sort the buffer, and
		 * remove duplicate entries by setting the inode
		 * number to zero.
		 */

		do {
			/*
			 * Always make at least DIRBLKSIZ bytes
			 * available to _getdirentries
			 */
			if (space < DIRBLKSIZ) {
				space += incr;
				len += incr;
				buf = reallocf(buf, len);
				if (buf == NULL)
					goto fail;
				ddptr = buf + (len - space);
			}

			n = _getdirentries(fd, ddptr, space, &dirp->dd_seek);
			if (n > 0) {
				ddptr += n;
				space -= n;
			}
		} while (n > 0);

		ddeptr = ddptr;
		flags |= __DTF_READALL;

		/*
		 * Re-open the directory.
		 * This has the effect of rewinding back to the
		 * top of the union stack and is needed by
		 * programs which plan to fchdir to a descriptor
		 * which has also been read -- see fts.c.
		 */
		if (flags & DTF_REWIND) {
			(void)_close(fd);
			if ((fd = _open(name, O_RDONLY | O_DIRECTORY |
			    O_CLOEXEC)) == -1) {
				saved_errno = errno;
				free(buf);
				free(dirp);
				errno = saved_errno;
				return (NULL);
			}
		}

		/*
		 * There is now a buffer full of (possibly) duplicate
		 * names.
		 */
		dirp->dd_buf = buf;

		/*
		 * Go round this loop twice...
		 *
		 * Scan through the buffer, counting entries.
		 * On the second pass, save pointers to each one.
		 * Then sort the pointers and remove duplicate names.
		 */
		for (dpv = 0;;) {
			n = 0;
			ddptr = buf;
			while (ddptr < ddeptr) {
				struct dirent *dp;

				dp = (struct dirent *) ddptr;
				if ((long)dp & 03L)
					break;
				if ((dp->d_reclen <= 0) ||
				    (dp->d_reclen > (ddeptr + 1 - ddptr)))
					break;
				ddptr += dp->d_reclen;
				if (dp->d_fileno) {
					if (dpv)
						dpv[n] = dp;
					n++;
				}
			}

			if (dpv) {
				struct dirent *xp;

				/*
				 * This sort must be stable.
				 */
				mergesort(dpv, n, sizeof(*dpv),
				    opendir_compar);

				dpv[n] = NULL;
				xp = NULL;

				/*
				 * Scan through the buffer in sort order,
				 * zapping the inode number of any
				 * duplicate names.
				 */
				for (n = 0; dpv[n]; n++) {
					struct dirent *dp = dpv[n];

					if ((xp == NULL) ||
					    strcmp(dp->d_name, xp->d_name)) {
						xp = dp;
					} else {
						dp->d_fileno = 0;
					}
					if (dp->d_type == DT_WHT &&
					    (flags & DTF_HIDEW))
						dp->d_fileno = 0;
				}

				free(dpv);
				break;
			} else {
				dpv = malloc((n+1) * sizeof(struct dirent *));
				if (dpv == NULL)
					break;
			}
		}

		dirp->dd_len = len;
		dirp->dd_size = ddptr - dirp->dd_buf;
	} else {
		dirp->dd_len = incr;
		dirp->dd_size = 0;
		dirp->dd_buf = malloc(dirp->dd_len);
		if (dirp->dd_buf == NULL)
			goto fail;
		dirp->dd_seek = 0;
		flags &= ~DTF_REWIND;
	}

	dirp->dd_loc = 0;
	dirp->dd_fd = fd;
	dirp->dd_flags = flags;
	dirp->dd_lock = NULL;

	/*
	 * Set up seek point for rewinddir.
	 */
	dirp->dd_rewind = telldir(dirp);

	return (dirp);

fail:
	saved_errno = errno;
	free(dirp);
	(void)_close(fd);
	errno = saved_errno;
	return (NULL);
}

#endif // PS4_PROVIDE_DIR





#endif //TARGET_PS4