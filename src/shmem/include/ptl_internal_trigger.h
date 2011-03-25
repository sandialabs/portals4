#ifndef PTL_INTERNAL_TRIGGER_H
#define PTL_INTERNAL_TRIGGER_H

#include "ptl_visibility.h"

typedef enum {
    CTINC,
    CTSET
} ptl_internal_trigtype_t;

typedef struct ptl_internal_trigger_s {
    struct ptl_internal_trigger_s *next; // this is for the pool of triggers
    ptl_size_t                     next_threshold;
    ptl_size_t                     threshold;
    ptl_internal_trigtype_t        type;
    union {
        struct {
            ptl_handle_ct_t ct_handle;
            ptl_ct_event_t  increment;
        } ctinc;
        struct {
            ptl_handle_ct_t ct_handle;
            ptl_ct_event_t  newval;
        } ctset;
    } t;
} ptl_internal_trigger_t;

void INTERNAL PtlInternalTriggerPull(ptl_internal_trigger_t *t);

#endif /* ifndef PTL_INTERNAL_TRIGGER_H */
/* vim:set expandtab: */
