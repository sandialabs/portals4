#ifndef PTL_INTERNAL_ALIGNMENT_H
#define PTL_INTERNAL_ALIGNMENT_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef SANDIA_ALIGNEDDATA_ALLOWED
# define ALIGNED(x) __attribute__((aligned(x)))
#else
# define ALIGNED(x)
#endif

#include <string.h>                    /* for memset() */

#if defined(HAVE_MEMALIGN)
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
# define ALIGNED_CALLOC(ret, align, count, size) do { \
        (ret) = memalign((align), (count) * (size)); \
        memset((void*)(ret), 0, (count) * (size)); \
} while (0)
# define ALIGNED_MALLOC(ret, align, size)        do { \
        (ret) = memalign((align), (size)); \
} while (0)
# define ALIGNED_FREE(x, align)                  free((x))
#elif defined(HAVE_POSIX_MEMALIGN)
/* may have to define _XOPEN_SOURCE 600 */
# include <stdlib.h>
# define ALIGNED_CALLOC(ret, align, count, size) do { \
        posix_memalign((void**)&(ret), (align), (count) * (size)); \
        memset((void*)(ret), 0, (count) * (size)); \
} while (0)
# define ALIGNED_MALLOC(ret, align, size)        do { \
        posix_memalign((void**)&(ret), (align), (size)); \
} while (0)
# define ALIGNED_FREE(x, align)                  free((x))
#else /* if defined(HAVE_MEMALIGN) */
# define ALIGNED_CALLOC(ret, align, count, size) do { \
        uintptr_t tmp1 = (uintptr_t)malloc((count) * (size) + (align)); \
        tmp1 &= ~(uintptr_t)(align - 1); \
        tmp1 += align; \
        (ret) = tmp1; \
        memset((void*)(ret), 0, (count) * (size)); \
} while (0)
# define ALIGNED_MALLOC(ret, align, size)        do { \
        uintptr_t tmp1 = (uintptr_t)malloc((size) + (align)); \
        tmp1 &= ~(uintptr_t)(align - 1); \
        tmp1 += align; \
        (ret) = tmp1; \
} while (0)
# define ALIGNED_FREE(x, align)                  free(((char*)(x)) - (align))
#endif /* if defined(HAVE_MEMALIGN) */

#endif /* ifndef PTL_INTERNAL_ALIGNMENT_H */
/* vim:set expandtab */
