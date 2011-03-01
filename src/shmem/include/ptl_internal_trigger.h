#ifndef PTL_INTERNAL_TRIGGER_H
#define PTL_INTERNAL_TRIGGER_H

typedef struct ptl_internal_trigger_s {
    struct ptl_internal_trigger_s *next;
    ptl_size_t threshold;
} ptl_internal_trigger_t;

#endif
