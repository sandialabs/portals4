#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <xpmem.h>
#include <unistd.h>

#include "ptl_internal_alignment.h"
#include "ptl_command_queue.h"

typedef struct ptl_cq_t ptl_cq_t;

struct ptl_cqe_xpmem_t {
    struct ptl_cqe_xpmem_t *next;
    int msg_len;
    int home_index;
    unsigned char data[];
};
typedef struct ptl_cqe_xpmem_t ptl_cqe_xpmem_t;

struct ptl_cq_info_t
{
    int index;
    xpmem_segid_t segid;
    size_t xpmem_offset;
    size_t xpmem_len;
};

struct ptl_cq_connection_t {
    xpmem_segid_t segid;
    xpmem_apid_t apid;
    ptl_cq_t *cq_ptr; /* local pointer to remote cq */
};
typedef struct ptl_cq_connection_t ptl_cq_connection_t;

struct ptl_cq_cb_t {
    uint64_t head;
    uint8_t pad1[CACHELINE_WIDTH - sizeof(uint64_t)];
    uint64_t tail;
    uint8_t pad2[CACHELINE_WIDTH - sizeof(uint64_t)];
    uint64_t mask;
    uint64_t num_entries;
    uint64_t data[];
};
typedef struct ptl_cq_cb_t ptl_cq_cb_t ALIGNED (CACHELINE_WIDTH);

struct ptl_cq_t {
    xpmem_segid_t segid;
    int my_index;
    size_t xpmem_len;
    size_t xpmem_offset;
    ptl_cq_connection_t connections[MC_PEER_COUNT];
    size_t entry_len;
    ptl_cqe_xpmem_t *freeq;
    ptl_cq_cb_t cb;
};

#define MKREM(my_index, offset) ((((uint64_t) my_index) << 56) | offset)
#define REM2ID(rem) (((uint64_t) rem) >> 56)
#define REM2OFF(rem) (rem & 0x00FFFFFFFFFFFFFFULL)
#define PTR2OFF(base, ptr) ((ptrdiff_t) ((char*) ptr - (char*) base))
#define OFF2PTR(base, off) ((char*) base + off)

int
ptl_cq_create(size_t entry_size, 
              int send_queue_size, int recv_queue_size,
              int my_index, ptl_cq_handle_t *cq_h)
{
    ptl_cq_t *cq;
    int i, num_pages = 1;
    size_t cq_base_page, len, pow2;
    ptl_cqe_xpmem_t *cqe_ptr, *last;
    long page_size = sysconf(_SC_PAGESIZE);

    pow2 = 1;
    while (pow2 < recv_queue_size) pow2 <<= 1;
    recv_queue_size = pow2;

    len = sizeof(ptl_cq_t) + 
        recv_queue_size * sizeof(uint64_t) + 
        send_queue_size * (sizeof(ptl_cqe_xpmem_t) + entry_size);

    cq = malloc(len);
    if (NULL == cq) return -1;
    *cq_h = cq;

    cq_base_page = (uintptr_t) cq;
    cq_base_page /= page_size;
    cq_base_page *= page_size;
    while (num_pages * page_size < len) num_pages++;

    cq->xpmem_len = num_pages * page_size;
    cq->xpmem_offset = (char*) cq - (char*) cq_base_page;
    cq->segid = xpmem_make((void*) cq_base_page, cq->xpmem_len,
                           XPMEM_PERMIT_MODE, (void*)0666);
    if (-1 == cq->segid) {
        return -1;
        free(cq);
    }

    cq->my_index = my_index;
    for (i = 0 ; i < MC_PEER_COUNT ; ++i) {
        cq->connections[i].segid = -1;
        cq->connections[i].apid = -1;
        cq->connections[i].cq_ptr = NULL;
    }
    cq->entry_len = entry_size;

    cq->cb.head = 0;
    cq->cb.tail = 0;
    cq->cb.mask = recv_queue_size - 1;
    cq->cb.num_entries = recv_queue_size;

    cqe_ptr = (ptl_cqe_xpmem_t*) ((char*) (cq + 1) + 
                                  recv_queue_size * sizeof(uint64_t));
    cq->freeq = cqe_ptr;
    last = NULL;

    for (i = 0 ; i < send_queue_size ; ++i) {
        cqe_ptr->next = NULL;
        cqe_ptr->home_index = cq->my_index;

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
    (*info)->index = cq_h->my_index;
    (*info)->segid = cq_h->segid;
    (*info)->xpmem_offset = cq_h->xpmem_offset;
    (*info)->xpmem_len = cq_h->xpmem_len;
    *info_len = sizeof(ptl_cq_info_t);
    
    return 0;
}


int
ptl_cq_attach(ptl_cq_handle_t cq_h, ptl_cq_info_t *info)
{
    ptl_cq_connection_t *conn;
    struct xpmem_addr addr;

    conn = &cq_h->connections[info->index];

    conn->segid = info->segid;
    conn->apid = xpmem_get(info->segid, XPMEM_RDWR, 
                           XPMEM_PERMIT_MODE, (void*)0666);
    if (conn->apid < 0) {
        int err = errno;
        free(conn);
        errno = err;
        return -1;
    }

    addr.apid = conn->apid;
    addr.offset = 0;

    conn->cq_ptr = xpmem_attach(addr, info->xpmem_len, NULL);
    if ((size_t) conn->cq_ptr == XPMEM_MAXADDR_SIZE) return -1;
    conn->cq_ptr = (ptl_cq_t*)((char*) conn->cq_ptr + info->xpmem_offset);

    return 0;
}


int
ptl_cq_destroy(ptl_cq_handle_t cq_h)
{
    int i;

    for (i = 0 ; i < MC_PEER_COUNT ; ++i) {
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

    if (tmp->home_index != cq_h->my_index) {
        return 1;
    }

    do {
        tmp->next = cq_h->freeq;
    } while (!__sync_bool_compare_and_swap(&cq_h->freeq,
                                           tmp->next,
                                           tmp));

    return 0;
}


int
ptl_cq_entry_send(ptl_cq_handle_t cq_h, int index, ptl_cqe_t *entry, size_t len)
{
    uint64_t tmp, data;
    ptl_cqe_xpmem_t *real = (ptl_cqe_xpmem_t*)
        ((char*)entry - offsetof(ptl_cqe_xpmem_t, data));
    ptl_cq_t *rem_cq;

    real->next = real; /* stash my real pointer for return */
    real->msg_len = len;
    data = MKREM(cq_h->my_index, 
                 PTR2OFF(cq_h, real));
    rem_cq = cq_h->connections[index].cq_ptr;

    __sync_synchronize();
    do {
        tmp = rem_cq->cb.head;
        if (tmp >= rem_cq->cb.tail + rem_cq->cb.num_entries) return 1;
    } while (!__sync_bool_compare_and_swap(&rem_cq->cb.head,
                                           tmp,
                                           tmp + 1));

    tmp = tmp & rem_cq->cb.mask;
    rem_cq->cb.data[tmp] = data;

    return 0;
}


int
ptl_cq_entry_send_block(ptl_cq_handle_t cq_h, int index, 
                        ptl_cqe_t *entry, size_t len)
{
    uint64_t tmp, data;
    ptl_cqe_xpmem_t *real = (ptl_cqe_xpmem_t*)
        ((char*)entry - offsetof(ptl_cqe_xpmem_t, data));
    ptl_cq_t *rem_cq;

    real->next = real; /* stash my real pointer for return */
    real->msg_len = len;
    data = MKREM(cq_h->my_index, 
                 PTR2OFF(cq_h, real));
    rem_cq = cq_h->connections[index].cq_ptr;

    __sync_synchronize();
    do {
        tmp = rem_cq->cb.head;
        if (tmp >= rem_cq->cb.tail + rem_cq->cb.num_entries) continue;
    } while (!__sync_bool_compare_and_swap(&rem_cq->cb.head,
                                           tmp,
                                           tmp + 1));

    tmp = tmp & rem_cq->cb.mask;
    rem_cq->cb.data[tmp] = data;

    return 0;
}


int
ptl_cq_entry_recv(ptl_cq_handle_t cq_h, ptl_cqe_t *entry)
{
    uint64_t tmp, data;
    int id;
    ptl_cqe_xpmem_t *real, *rem;
    ptl_cq_t *rem_cq;

    __sync_synchronize();
    do {
        tmp = cq_h->cb.tail;
        if (tmp == cq_h->cb.head) return 1;
    } while (!__sync_bool_compare_and_swap(&cq_h->cb.tail,
                                           tmp,
                                           tmp + 1));

    while (0 == (data = cq_h->cb.data[(cq_h->cb.mask & tmp)])) {
        __sync_synchronize();
    }
    cq_h->cb.data[(cq_h->cb.mask & tmp)] = 0;
    __sync_synchronize();

    id = REM2ID(data);
    real = (ptl_cqe_xpmem_t*) 
        OFF2PTR(cq_h->connections[id].cq_ptr, REM2OFF(data));

    memcpy(entry, 
           real->data,
           real->msg_len);

    /* put back on free list */
    rem_cq = cq_h->connections[id].cq_ptr;
    rem = real->next;
    do {
        real->next = rem_cq->freeq;
    } while (!__sync_bool_compare_and_swap(&rem_cq->freeq,
                                           real->next,
                                           rem));

    return 0;
}
