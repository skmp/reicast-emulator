#ifndef MMAN_H
#define MMAN_H

#ifdef __cplusplus
extern "C"
{
#endif

// needed is most of the same files that need mman
char 	*stpcpy (char *__restrict, const char *__restrict);

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <stdlib.h>

#define PROT_NONE 0b000
#define PROT_READ 0b001
#define PROT_WRITE 0b010
#define PROT_EXEC 0b100
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS

#define MAP_FAILED ((void *)-1)

static inline void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    return (void*)-1;
}

static inline int mprotect(void *addr, size_t len, int prot)
{
    return 0;
}

static inline int munmap(void *addr, size_t len)
{
    return 0;
}

#ifdef __cplusplus
};
#endif

#endif // MMAN_H