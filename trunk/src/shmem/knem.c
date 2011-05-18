/*
 * KNEM - http://runtime.bordeaux.inria.fr/knem/
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <stdio.h>                      /* for perror() */
#include <sys/stat.h>                   /* for open() */
#include <fcntl.h>                      /* for open() */
#include <assert.h>                     /* for assert() */
#include <stdlib.h>                     /* for abort() */

/* Internals */
#include "ptl_internal_knem.h"

static int knem_fd          = -2;
static int knem_init_called = 0;

#ifdef PARANOID
static void init_check(const char *func)
{
    if (!knem_init_called) {
        fprintf(stderr, "PORTALS4-> %s: ERROR: called before knem_init!\n",
                func);
        abort();
    }
}

# define PARANOID_STEP(x) x
#else /* ifdef PARANOID */
# define PARANOID_STEP(x)
#endif /* ifdef PARANOID */

void INTERNAL knem_init(void)
{
    if (-2 == knem_fd) {
        knem_fd = open("/dev/knem", O_RDWR);
        if (-1 == knem_fd) {
            perror("open");
            abort();
        }
    }

    knem_init_called = 1;
}

uint64_t INTERNAL knem_register_singleuse(void      *data,
                                          ptl_size_t len,
                                          int        prot)
{
    struct knem_cmd_create_region create;
    struct knem_cmd_param_iovec   iov;
    int                           err;

    PARANOID_STEP(init_check(__func__); )

    iov.base           = (uintptr_t)data;
    iov.len            = len;
    create.iovec_array = (uintptr_t)&iov;
    create.iovec_nr    = 1;
    create.flags       = KNEM_FLAG_SINGLEUSE;
    create.protection  = prot;

    err = ioctl(knem_fd, KNEM_CMD_CREATE_REGION, &create);
    if (err < 0) {
        fprintf(stderr, "PORTALS4-> %s: KNEM create region failed, err = %d\n",
                __func__, err);
        abort();
    }

    assert(create.cookie != 0);
    return create.cookie;
}

uint64_t INTERNAL knem_register(void      *data,
                                ptl_size_t len,
                                int        prot)
{
    struct knem_cmd_create_region create;
    struct knem_cmd_param_iovec   iov;
    int                           err;

    PARANOID_STEP(init_check(__func__); )

    iov.base           = (uintptr_t)data;
    iov.len            = len;
    create.iovec_array = (uintptr_t)&iov;
    create.iovec_nr    = 1;
    create.flags       = 0;
    create.protection  = prot;

    err = ioctl(knem_fd, KNEM_CMD_CREATE_REGION, &create);
    if (err < 0) {
        fprintf(stderr, "PORTALS4-> KNEM create region failed, err = %d\n",
                err);
        abort();
    }

    assert(create.cookie != 0);
    return create.cookie;
}

void INTERNAL knem_unregister(uint64_t cookie)
{
    int err;

    PARANOID_STEP(init_check(__func__); )

    err = ioctl(knem_fd, KNEM_CMD_DESTROY_REGION, &cookie);

    if (err < 0) {
        fprintf(stderr, "PORTALS4-> KNEM destroy region failed, err = %d\n",
                err);
        abort();
    }
}

/* copy data *from* a KNEM region to a local buffer */
size_t INTERNAL knem_copy_from(void    *dst,
                               uint64_t cookie,
                               uint64_t off,
                               size_t   len)
{
    struct knem_cmd_inline_copy icopy;
    struct knem_cmd_param_iovec iov;
    int                         err;

    PARANOID_STEP(init_check(__func__); )

    iov.base                = (uintptr_t)dst;
    iov.len                 = len;
    icopy.local_iovec_array = (uintptr_t)&iov;
    icopy.local_iovec_nr    = 1;
    icopy.remote_cookie     = cookie;
    icopy.remote_offset     = off;
    icopy.write             = 0;
    icopy.flags             = 0;

    err = ioctl(knem_fd, KNEM_CMD_INLINE_COPY, &icopy);
    if (err < 0) {
        fprintf(stderr, "PORTALS4-> KNEM inline copy failed, err = %d\n", err);
        abort();
    }
    if (icopy.current_status != KNEM_STATUS_SUCCESS) {
        fprintf(stderr, "PORTALS4-> KNEM inline copy status "
                        "(%u) != KNEM_STATUS_SUCCESS\n",
                icopy.current_status);
        abort();
    }

    return len;
}

/* copy data *to* a KNEM region from a local buffer */
size_t INTERNAL knem_copy_to(uint64_t cookie,
                             uint64_t off,
                             void    *src,
                             size_t   len)
{
    struct knem_cmd_inline_copy icopy;
    struct knem_cmd_param_iovec iov;
    int                         err;

    PARANOID_STEP(init_check(__func__); )

    iov.base                = (uintptr_t)src;
    iov.len                 = len;
    icopy.local_iovec_array = (uintptr_t)&iov;
    icopy.local_iovec_nr    = 1;
    icopy.remote_cookie     = cookie;
    icopy.remote_offset     = off;
    icopy.write             = 1;
    icopy.flags             = 0;

    err = ioctl(knem_fd, KNEM_CMD_INLINE_COPY, &icopy);
    if (err < 0) {
        fprintf(stderr, "PORTALS4-> KNEM inline copy failed, err = %d\n", err);
        abort();
    }
    if (icopy.current_status != KNEM_STATUS_SUCCESS) {
        fprintf(stderr, "PORTALS4-> KNEM inline copy status (%u) != "
                        "KNEM_STATUS_SUCCESS\n", icopy.current_status);
        abort();
    }

    return len;
}

void INTERNAL *knem_attach(uint64_t cookie)
{
    /* KNEM requires a copy */
    return NULL;
}

/* vim:set expandtab: */
