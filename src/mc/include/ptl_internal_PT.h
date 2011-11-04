#ifndef PTL_INTERNAL_PT_H
#define PTL_INTERNAL_PT_H

#include <pthread.h>
#include <stdint.h>                    /* for uint32_t */

#include "ptl_internal_alignment.h"
//#include "ptl_internal_locks.h"

typedef struct {
    uint32_t        status;
    ptl_handle_eq_t EQ;
#if 0
    PTL_LOCK_TYPE   lock;
    unsigned int    options;
    struct PTqueue {
        void *head, *tail;
    } priority,
      overflow,
      buffered_headers;
#endif
} ptl_table_entry_t ALIGNED (64);

void PtlInternalPTInit(ptl_table_entry_t *t);
int  PtlInternalPTValidate(ptl_table_entry_t *t);

#endif /* ifndef PTL_INTERNAL_PT_H */
/* vim:set expandtab: */

