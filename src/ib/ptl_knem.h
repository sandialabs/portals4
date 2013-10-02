/*
 * ptl_knem.h - KNEM functions
 */

#ifndef PTL_KNEM_H
#define PTL_KNEM_H

#if (WITH_TRANSPORT_SHMEM && USE_KNEM)

int knem_init(ni_t *ni);
void knem_fini(ni_t *ni);
uint64_t knem_register(ni_t *ni, void *data, ptl_size_t len, int prot);
void knem_unregister(ni_t *ni, uint64_t cookie);
size_t knem_copy(ni_t *ni, uint64_t scookie, uint64_t soffset,
                 uint64_t dcookie, uint64_t doffset, size_t length);
#else

static inline int knem_init(ni_t *ni)
{
    return PTL_OK;
}

static inline void knem_fini(ni_t *ni)
{
}

static inline uint64_t knem_register(ni_t *ni, void *data, ptl_size_t len,
                                     int prot)
{
    return 1;
}

static inline void knem_unregister(ni_t *ni, uint64_t cookie)
{
}

#endif

#endif
