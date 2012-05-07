/*
 * XFE: Transfer Engine abstraction layer
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>             /* for fprintf(), stderr */
#include <stdlib.h>            /* for abort() */
#include <inttypes.h>          /* for PRIx64 */

#include "ptl_internal_transfer_engine.h"

#if 0
extern size_t proc_number;
# define xfe_printf(format, ...) if (getenv("XFE_VERBOSE")) { \
                                    printf("%u +> " format, \
                                    (unsigned int)proc_number, \
                                    ## __VA_ARGS__); \
                                 }
#else
# define xfe_printf(format, ...)
#endif

/*
 * Decide which transfer engine to use, if any.
 */
#if defined(USE_KNEM)
void INTERNAL xfe_init(void)
{
    xfe_printf("XFE: called xfe_init()\n");
    knem_init();
}

uint64_t INTERNAL xfe_register_singleuse(void *data,
                                         ptl_size_t length,
                                         int prot)
{
    xfe_printf("XFE: called xfe_register_singleuse(data pointer=%p,"
               " length=%lu, protection=%d)\n", data, length, prot);
    return knem_register_singleuse(data, length, prot);
}

uint64_t INTERNAL xfe_register(void *data, ptl_size_t length, int prot)
{
    xfe_printf("XFE: called xfe_register(data pointer=%p, length=%lu,"
               " protection=%d)\n", data, length, prot);
    return knem_register(data, length, prot);
}

void INTERNAL xfe_unregister(uint64_t handle)
{
    xfe_printf("XFE: called xfe_unregister(handle=0x%016" PRIx64 ")\n", handle);
    knem_unregister(handle);
}

size_t INTERNAL xfe_copy_from(void *dst, uint64_t handle,
                              uint64_t offset, size_t length)
{
    xfe_printf("XFE: called xfe_copy_from(dst pointer=%p, handle=0x%016" PRIx64
               ", offset=0x%016" PRIx64 ", length=0x%016" PRIx64 ")\n",
               dst, handle, offset, length);
    return knem_copy_from(dst, handle, offset, length);
}

size_t INTERNAL xfe_copy_to(uint64_t handle, uint64_t offset,
                            void *src, size_t length)
{
    xfe_printf("XFE: called xfe_copy_to(handle=0x%016" PRIx64 ", offset=0x%016"
               PRIx64 ", src pointer=%p, length=0x%016" PRIx64 ")\n",
               handle, offset, src, length);
    return knem_copy_to(handle, offset, src, length);
}

void INTERNAL *xfe_attach(uint64_t handle)
{
    xfe_printf("XFE: called xfe_attach(handle=0x%016" PRIx64 ")\n", handle);
    return knem_attach(handle);
}

#elif defined(USE_XPMEM)
# error "TODO: Add XPMEM support to transfer_engine"

#elif defined(USE_CMA)
# error "TODO: Add CMA support to transfer_engine"

#elif defined(USE_LIMIC2)
# error "TODO: Add LiMIC2 support to transfer_engine"

#else /* if defined(USE_KNEM) */
/* No transfer engine detected.  Error out if anyone makes an XFE call. */
static void xfe_missing(const char *func)
{
    fprintf(stderr, "PORTALS4-> %s: ERROR: No transfer engine detected.\n", func);
    abort();
}
void INTERNAL xfe_init(void)
{
    xfe_missing(__func__);
}
uint64_t INTERNAL xfe_register_singleuse(void *data,
                                         ptl_size_t length,
                                         int prot)
{
    xfe_missing(__func__); return 0UL;
}
uint64_t INTERNAL xfe_register(void *data, ptl_size_t length, int prot)
{
    xfe_missing(__func__);
    return 0UL;
}
void INTERNAL xfe_unregister(uint64_t handle)
{
    xfe_missing(__func__);
}
size_t INTERNAL xfe_copy_from(void *dst, uint64_t handle,
                              uint64_t offset, size_t length)
{
    xfe_missing(__func__);
    return 0UL;
}
size_t INTERNAL xfe_copy_to(uint64_t handle, uint64_t offset,
                            void *src, size_t length)
{
    xfe_missing(__func__);
    return 0UL;
}
void INTERNAL * xfe_attach(uint64_t handle)
{
    xfe_missing(__func__);
    return NULL;
}
#endif /* if defined(USE_KNEM) */
/* vim:set expandtab: */
