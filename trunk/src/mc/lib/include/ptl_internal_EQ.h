#ifndef PTL_INTERNAL_EQ_H
#define PTL_INTERNAL_EQ_H

#include "shared/ptl_circular_buffer.h"

struct ptl_internal_eq_t {
    char in_use;
    ptl_circular_buffer_t *cb;
};
typedef struct ptl_internal_eq_t ptl_internal_eq_t;

#ifndef NO_ARG_VALIDATION
int PtlInternalEQHandleValidator(ptl_handle_eq_t handle,
                                 int             none_ok);
#endif

#endif
/* vim:set expandtab: */
