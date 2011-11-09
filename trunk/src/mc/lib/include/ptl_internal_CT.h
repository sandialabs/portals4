#ifndef PTL_INTERNAL_CT_H
#define PTL_INTERNAL_CT_H

#include "ptl_visibility.h"

struct ptl_internal_ct_t {
    char in_use;
};
typedef struct ptl_internal_ct_t ptl_internal_ct_t;

#ifndef NO_ARG_VALIDATION
int INTERNAL  PtlInternalCTHandleValidator(ptl_handle_ct_t handle,
                                           uint_fast8_t    none_ok);
#endif

#endif

/* vim:set expandtab: */
