#ifndef MC_COMMAND_QUEUE_XPMEM_H
#define MC_COMMAND_QUEUE_XPMEM_H

struct ptl_cqe_t
{
    int offset;
    unsigned char buffer[];
};


struct ptl_cq_info_t
{
    int foobar;
};


struct ptl_cq_t
{
    int foobar;
};

#endif
