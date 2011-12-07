
#ifndef MC_PPE_TRIGGERED_H
#define MC_PPE_TRIGGERED_H

#include "shared/ptl_command_queue_entry.h"
#include "shared/ptl_double_list.h"
#include "portals4.h"

struct ptl_triggered_op_t {
    ptl_double_list_item_t  list;
    int         type;
    ptl_size_t  threshold;
    int         index;
    union {
        ptl_ctop_t   ct_op;
    } u;
};
typedef struct ptl_triggered_op_t ptl_triggered_op_t;

void PtlInternalCTPullTriggers( ptl_ppe_ni_t *, ptl_handle_ct_t );

#endif
