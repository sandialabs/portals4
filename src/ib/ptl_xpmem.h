#ifndef PTL_XPMEM_H
#define PTL_XPMEM_H


#if WITH_PPE

#include "xpmem.h"

/* XPMEM mapping. Maybe this structure should be split into 2
 * differents ones: one for the client and one for the PPE. */
struct xpmem_map {
    /* From source process. */
    const void *source_addr;
    size_t size;

    off_t offset;               /* from start of segid to source_addr */
    xpmem_segid_t segid;

    /* On dest process. */
    void *ptr_attach;           /* registered address with xpmem_attach */

    /* Both. */
    struct xpmem_addr addr;
};

#endif

#endif /* PTL_XPMEM_H */
