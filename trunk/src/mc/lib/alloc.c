#include "config.h"


#include <malloc.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/mman.h>
#if defined(HAVE___MUNMAP)
/* here so we only include others if we absolutely have to */
#elif defined(HAVE_SYSCALL)
#include <syscall.h>
#include <unistd.h>
#endif
#if defined(HAVE_DLSYM)
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <dlfcn.h>
#endif
#include <stdio.h>

#include "alloc.h"

void* dlmalloc(size_t, const void *);
void  dlfree(void*, const void *);
void* dlrealloc(void*, size_t, const void *);
void* dlmemalign(size_t, size_t, const void *);

#if defined(HAVE___MMAP)
int  __mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset);
#endif
#if defined(HAVE___MUNMAP)
int  __munmap(void* addr, size_t len);
#endif

static int initialized = 0;

static void 
alloc_init_hook(void)
{
    __free_hook = dlfree;
    __malloc_hook = dlmalloc;
    __realloc_hook = dlrealloc;
    __memalign_hook = dlmemalign;
}

void (*__malloc_initialize_hook)(void) = alloc_init_hook;


void *
mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
{
#if !defined(HAVE___MMAP) && defined(HAVE_DLSYM)
    static void* (*realmmap)(void*, size_t, int, int, int, off_t);
#endif
    void *ret;

#if defined(HAVE___MUNMAP)
    ret = __mmap(start, length, prot, flags, fd, offset);
#elif defined(HAVE_DLSYM)
    if (NULL == realmmap) {
        union { 
            void* (*mmap_fp)(void*, size_t, int, int, int, off_t);
            void *mmap_p;
        } tmp;

        tmp.mmap_p = dlsym(RTLD_NEXT, "mmap");
        realmmap = tmp.mmap_fp;
    }

    ret = realmmap(start, length, prot, flags, fd, offset);
#else
    #error "Can not determine how to call munmap"
#endif

    alloc_add_mem(ret, length);

    return ret;
}


int
munmap(void* addr, size_t len)
{
#if !defined(HAVE___MUNMAP) && \
    !(defined(HAVE_SYSCALL) && defined(__NR_munmap)) && defined(HAVE_DLSYM)
    static int (*realmunmap)(void*, size_t);
#endif

    alloc_remove_mem(addr, len);

#if defined(HAVE___MUNMAP)
    return __munmap(addr, len);
#elif defined(HAVE_SYSCALL) && defined(__NR_munmap)
    return syscall(__NR_munmap, addr, len);
#elif defined(HAVE_DLSYM)
    if (NULL == realmunmap) {
        union { 
            int (*munmap_fp)(void*, size_t);
            void *munmap_p;
        } tmp;

        tmp.munmap_p = dlsym(RTLD_NEXT, "munmap");
        realmunmap = tmp.munmap_fp;
    }

    return realmunmap(addr, len);
#else
    #error "Can not determine how to call munmap"
#endif
}


int
alloc_init(void)
{
    initialized = 1;
    return 0;
}


int
alloc_fini(void)
{
    initialized = 0;
    return 0;
}


void
alloc_add_mem(void *start, size_t length)
{
    if (initialized) printf("Allocation: %p, %d\n", start, (int) length);
}


void
alloc_remove_mem(void *start, size_t length)
{
    if (initialized) printf("Deallocation: %p, %d\n", start, (int) length);
}
