#ifndef PTL_INTERNAL_NIT_H
#define PTL_INTERNAL_NIT_H

#include <pthread.h>

typedef struct {
    pthread_mutex_t *lock;
    void *mle;
} ptl_table_entry_t;

typedef struct {
    ptl_table_entry_t *tables[4];
    unsigned int enabled;	// mask
} ptl_internal_nit_t;

extern ptl_internal_nit_t nit;
extern ptl_ni_limits_t nit_limits;

#endif
