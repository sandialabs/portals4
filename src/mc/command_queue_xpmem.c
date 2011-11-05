#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <xpmem.h>

#include "command_queue.h"

#define CQ_MAX_CONNECTIONS 32

typedef struct ptl_cq_t ptl_cq_t;

struct ptl_cqe_xpmem_t {
    struct ptl_cqe_xpmem_t *next;
    int home_index;
    unsigned char data[];
};
typedef struct ptl_cqe_xpmem_t ptl_cqe_xpmem_t;

struct ptl_cq_info_t
{
    int index;
    xpmem_segid_t segid;
    size_t len;
};

struct ptl_cq_connection_t {
    xpmem_segid_t segid;
    xpmem_apid_t apid;
    ptl_cq_t *cq_ptr; /* local pointer */
};
typedef struct ptl_cq_connection_t ptl_cq_connection_t;

struct ptl_cq_t {
    xpmem_segid_t segid;
    int my_index;
    size_t len;
    int connection_count;
    ptl_cq_connection_t connections[CQ_MAX_CONNECTIONS];
    ptl_cqe_xpmem_t *freeq;
};


int
ptl_cq_create(size_t entry_size, int num_entries, 
              int my_index, ptl_cq_handle_t *cq_h)
{
    ptl_cq_t *cq;
    int i;
    size_t len = sizeof(ptl_cq_t) + 
        num_entries * (sizeof(ptl_cqe_xpmem_t) + entry_size);
    ptl_cqe_xpmem_t *cqe_ptr, *last;

    cq = malloc(len);
    if (NULL == cq) return 1;
    *cq_h = cq;

    cq->segid = xpmem_make(cq, len, XPMEM_PERMIT_MODE, (void*)0666);
    if (-1 == cq->segid) {
        return 1;
        free(cq);
    }

    cq->len = len;
    cq->connection_count = 0;
    for (i = 0 ; i < CQ_MAX_CONNECTIONS ; ++i) {
        cq->connections[i].segid = -1;
        cq->connections[i].apid = -1;
        cq->connections[i].cq_ptr = NULL;
    }

    cqe_ptr = (ptl_cqe_xpmem_t*) (cq + 1);
    cq->freeq = cqe_ptr;
    last = NULL;

    for (i = 0 ; i < num_entries ; ++i) {
        cqe_ptr->next = NULL;

        if (NULL != last) last->next = cqe_ptr;
        last = cqe_ptr;
        
        cqe_ptr = (ptl_cqe_xpmem_t*) (((char*) (cqe_ptr + 1)) + entry_size);
    }
    
    return 0;
}


int
ptl_cq_info_get(ptl_cq_handle_t cq_h, ptl_cq_info_t **info, size_t *info_len)
{
    *info = (ptl_cq_info_t*) malloc(sizeof(ptl_cq_info_t));
    (*info)->segid = cq_h->segid;
    (*info)->len = cq_h->len;
    *info_len = sizeof(ptl_cq_info_t);
    
    return 0;
}


int
ptl_cq_attach(ptl_cq_handle_t cq_h, ptl_cq_info_t *info)
{
    ptl_cq_connection_t *conn;
    struct xpmem_addr addr;

    assert(cq_h->my_index != info->index);
    if (cq_h->my_index != 0) assert(info->index == 0);

    conn = &cq_h->connections[info->index];

    conn->segid = info->segid;
    conn->apid = xpmem_get(info->segid, XPMEM_RDWR, 
                           XPMEM_PERMIT_MODE, (void*)0666);
    if (conn->apid < 0) {
        free(conn);
        return 1;
    }

    addr.apid = conn->apid;
    addr.offset = 0;

    conn->cq_ptr = xpmem_attach(addr, info->len, NULL);
    if (conn->cq_ptr == NULL) return 1;

    return 0;
}


int
ptl_cq_destroy(ptl_cq_handle_t cq_h)
{
    int i;

    for (i = 0 ; i < CQ_MAX_CONNECTIONS ; ++i) {
        if (cq_h->connections[i].cq_ptr != NULL) {
            xpmem_detach(cq_h->connections[i].cq_ptr);
        }
        if (cq_h->connections[i].apid != -1) {
            xpmem_release(cq_h->connections[i].apid);
        }
    }

    if (cq_h->segid != -1) {
        xpmem_remove(cq_h->segid);
    }
    
    return 0;
}


int
ptl_cq_entry_alloc(ptl_cq_handle_t cq_h, ptl_cqe_t **entry)
{
    ptl_cqe_xpmem_t *tmp;

    while (NULL != (tmp = cq_h->freeq)) {
        if (__sync_bool_compare_and_swap(&cq_h->freeq,
                                         tmp,
                                         tmp->next)) {
            tmp->next = NULL;
            *entry = (ptl_cqe_t*) tmp->data;
            return 0;
        }
    }

    return 1;
}


int
ptl_cq_entry_free(ptl_cq_handle_t cq_h, ptl_cqe_t *entry)
{
    ptl_cqe_xpmem_t *tmp = (ptl_cqe_xpmem_t*)
        ((char*)entry - offsetof(ptl_cqe_xpmem_t, data));

    if (tmp->home_index == cq_h->my_index) {
        do {
            tmp->next = cq_h->freeq;
        } while (!__sync_bool_compare_and_swap(&cq_h->freeq,
                                               tmp->next,
                                               tmp));
    } else {
        /* BWB: FIX ME */
        return 1;
    }

    return 0;
}


int
ptl_cq_entry_send(ptl_cq_t *cq, ptl_cqe_t *entry)
{
    return 1;
}


int
ptl_cq_entry_recv(ptl_cq_t *cq, ptl_cqe_t *entry)
{
    return 1;
}
