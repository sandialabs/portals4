#ifndef PTL_INTERNAL_PT_H
#define PTL_INTERNAL_PT_H

#include <pthread.h>
#include <stdint.h>		       /* for uint32_t */

typedef struct {
    pthread_mutex_t lock;
    struct PTqueue {
	void *head, *tail;
    } priority,
            overflow;
    ptl_handle_eq_t EQ;
    enum { PT_FREE, PT_ENABLED, PT_DISABLED } status;
} ptl_table_entry_t;

void PtlInternalPTInit(
    ptl_table_entry_t * t);
int PtlInternalPTValidate(
    ptl_table_entry_t * t);

#endif
