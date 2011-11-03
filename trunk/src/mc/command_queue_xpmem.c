#include "config.h"

#include <stdlib.h>

#include <xpmem.h>

#include "command_queue.h"

#define CQ_MAX_CONNECTIONS 32

struct ptl_cq_connection_t {
    xpmem_apid_t apid;
    ptl_cq_t *cq_ptr;
};
typedef struct ptl_cq_connection_t ptl_cq_connection_t;

struct ptl_cq_t {
    xpmem_segid_t segid;
    size_t len;
    int connection_count;
    ptl_cq_connection_t connections[CQ_MAX_CONNECTIONS];
    ptl_cqe_t *entries;
};


int
ptl_cq_create(size_t entry_size, int num_entries, ptl_cq_handle_t *cq_h)
{
    ptl_cq_t *cq;
    int i;
    size_t len = sizeof(ptl_cq_t) + num_entries * (sizeof(ptl_cqe_t) + entry_size);
    char *entrybuf;
    ptl_cqe_t *cqe_ptr, *last = NULL;

    cq = malloc(len);
    if (NULL == cq) return 1;
    *cq_h = cq;

    cq->segid = -1;
    cq->len = len;
    cq->connection_count = 0;
    cq->entries = NULL;

    for (i = 0 ; i < CQ_MAX_CONNECTIONS ; ++i) {
        cq->connections[i].apid = -1;
        cq->connections[i].cq_ptr = NULL;
    }

    entrybuf = (char*) (cq + 1);
    cq->entries = (ptl_cqe_t*) entrybuf;
    cqe_ptr = (ptl_cqe_t*) entrybuf;

    for (i = 0 ; i < num_entries ; ++i) {
        cqe_ptr->next = NULL;

        if (NULL != last) last->next = cqe_ptr;
        last = cqe_ptr;
        
        cqe_ptr = (ptl_cqe_t*) (((char*) (cqe_ptr + 1)) + entry_size);
    }

    cq->segid = xpmem_make(cq, len, XPMEM_PERMIT_MODE, (void*)0666);
    if (-1 == cq->segid) return 1;
    
    return 0;
}


int
ptl_cq_info_get(ptl_cq_handle_t cq_h, ptl_cq_info_t *info)
{
    info->segid = cq_h->segid;
    info->len = cq_h->len;

    return 0;
}


int
ptl_cq_attach(ptl_cq_handle_t cq_h, ptl_cq_info_t *info)
{
    ptl_cq_connection_t *conn;
    struct xpmem_addr addr;

    /* BWB: FIX ME: this could be more thread safe... */
    conn = &cq_h->connections[cq_h->connection_count++];

    conn->apid = xpmem_get(info->segid, XPMEM_RDWR, XPMEM_PERMIT_MODE, (void*)0666);
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
    ptl_cqe_t *tmp;

    tmp = cq_h->entries;
    if (NULL == tmp) return 1;

    cq_h->entries = tmp->next;
    *entry = tmp;

    return 0;
}


int
ptl_cq_entry_free(ptl_cq_handle_t cq_h, ptl_cqe_t *entry)
{
    entry->next = cq_h->entries;
    cq_h->entries = entry;

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
