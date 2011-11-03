#ifndef MC_COMMAND_QUEUE_XPMEM_H
#define MC_COMMAND_QUEUE_XPMEM_H

#include <stdlib.h>
#include <xpmem.h>

struct ptl_cqe_t
{
    ptl_cqe_t *next;
    unsigned char buffer[];
};


struct ptl_cq_info_t
{
    xpmem_segid_t segid;
    size_t len;
};


#endif
