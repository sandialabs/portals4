#ifndef MC_PPE_PPE_XPMEM_H
#define MC_PPE_PPE_XPMEM_H

#include "shared/ptl_double_list.h"
#include "shared/ptl_free_list.h"
#include "shared/ptl_util.h"

struct ptl_ppe_xpmem_ptr_t {
    ptl_double_list_item_t base;
    void *xpmem_pointer;
    size_t xpmem_length;
    void *data;
    size_t length;
};
typedef struct ptl_ppe_xpmem_ptr_t ptl_ppe_xpmem_ptr_t;


struct ptl_ppe_xpmem_t {
    ptl_free_list_t freeq;
    ptl_double_list_t in_use;
    long page_size;
    xpmem_apid_t apid;
};
typedef struct ptl_ppe_xpmem_t ptl_ppe_xpmem_t;


int ppe_xpmem_init(ptl_ppe_xpmem_t *ppe_xpmem, 
                   xpmem_segid_t segid, long page_size);
int ppe_xpmem_fini(ptl_ppe_xpmem_t *ppe_xpmem);


static inline ptl_ppe_xpmem_ptr_t* 
ppe_xpmem_attach(ptl_ppe_xpmem_t *ppe_xpmem, void *remote_ptr, size_t len)
{
    struct xpmem_addr addr;
    ptl_ppe_xpmem_ptr_t *ptr;
    long buf_offset;

    ptr = ptl_free_list_alloc(&ppe_xpmem->freeq);
    if (NULL == ptr) return NULL;

    addr.apid = ppe_xpmem->apid;
    addr.offset = ptl_find_lower_boundary((long) remote_ptr, 
                                          ppe_xpmem->page_size);
    buf_offset = (long) remote_ptr - addr.offset;

    ptr->xpmem_length = ptl_find_upper_boundary(len + buf_offset,
                                                ppe_xpmem->page_size);

    ptr->xpmem_pointer = xpmem_attach(addr, ptr->xpmem_length, NULL);
    if (NULL == ptr->xpmem_pointer) {
        ptl_free_list_free(&ppe_xpmem->freeq, ptr);
        return NULL;
    }

    ptr->data = (char*) ptr->xpmem_pointer + buf_offset;
    ptr->length = len;

    ptl_double_list_insert_back(&ppe_xpmem->in_use, &ptr->base);

    return ptr;
}

static inline int
ppe_xpmem_detach(ptl_ppe_xpmem_t *ppe_xpmem, ptl_ppe_xpmem_ptr_t *ptr)
{
    int ret;

    ret = xpmem_detach(ptr->xpmem_pointer);

    ptl_double_list_remove_item(&ppe_xpmem->in_use, &ptr->base);
    ptl_free_list_free(&ppe_xpmem->freeq, ptr);

    return ret;
}

#endif
