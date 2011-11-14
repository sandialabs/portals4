#include "config.h"

#include <xpmem.h>

#include "ppe/ppe_xpmem.h"

int
ppe_xpmem_init(ptl_ppe_xpmem_t *ppe_xpmem, xpmem_segid_t segid, long page_size)
{
    int ret;

    ppe_xpmem->page_size = page_size;

    ppe_xpmem->apid = xpmem_get(segid,
                                XPMEM_RDWR, XPMEM_PERMIT_MODE, 
                                (void *) 0666);
    if (ppe_xpmem->apid < 0) {
        return -1;
    }

    ret = ptl_free_list_init(&ppe_xpmem->freeq, sizeof (ptl_ppe_xpmem_ptr_t));
    if (0 != ret) return ret;

    ret = ptl_double_list_init(&ppe_xpmem->in_use, 0);
    if (0 != ret) return ret;

    return 0;
}


int
ppe_xpmem_fini(ptl_ppe_xpmem_t *ppe_xpmem)
{
    int ret;

    ret = ptl_free_list_fini(&ppe_xpmem->freeq);
    if (0 != ret) return ret;

    ret = ptl_double_list_fini(&ppe_xpmem->in_use);
    if (0 != ret) return ret;

    xpmem_release(ppe_xpmem->apid);

    return 0;
}
