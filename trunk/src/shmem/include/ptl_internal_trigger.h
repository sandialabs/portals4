#ifndef PTL_INTERNAL_TRIGGER_H
#define PTL_INTERNAL_TRIGGER_H

#include "ptl_visibility.h"

typedef struct ptl_internal_trigger_s {
    struct ptl_internal_trigger_s *next; // this is for the pool of triggers
    ptl_size_t                     threshold;
} ptl_internal_trigger_t;

void INTERNAL PtlInternalTriggerPull(ptl_internal_trigger_t *t);

#endif
/* vim:set expandtab */
