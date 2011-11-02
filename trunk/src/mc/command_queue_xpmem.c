#include "config.h"

#include "command_queue.h"


int
ptl_cq_create(size_t entry_size, int num_entries, struct ptl_cq_t *cq)
{
    return 1;
}


int
ptl_cq_info_get(ptl_cq_info_t *info)
{
    return 1;
}


int
ptl_cq_attach(struct ptl_cq_info_t *info, ptl_cq_t *cq)
{
    return 1;
}


int
ptl_cq_destroy(struct ptl_cq_info_t *endpoint)
{
    return 1;
}


int
ptl_cq_entry_alloc(ptl_cq_t *cq, ptl_cqe_t** entry)
{
    return 1;
}


int
ptl_cq_entry_free(ptl_cq_t *cq, ptl_cqe_t* entry)
{
    return 1;
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
