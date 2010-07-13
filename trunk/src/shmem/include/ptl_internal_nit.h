#ifndef PTL_INTERNAL_NIT_H
#define PTL_INTERNAL_NIT_H

#include <pthread.h>
#include <stdint.h>		       /* for uint32_t */

typedef struct {
    pthread_mutex_t *lock;
    void *mle;
} ptl_table_entry_t;

typedef struct {
    ptl_table_entry_t *tables[4];
    uint32_t refcount[4];
    ptl_sr_value_t regs[4][2];
} ptl_internal_nit_t;

extern ptl_internal_nit_t nit;
extern ptl_ni_limits_t nit_limits;

#endif
