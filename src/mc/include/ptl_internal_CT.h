#ifndef PTL_INTERNAL_CT_H
#define PTL_INTERNAL_CT_H

#include "ptl_visibility.h"

#ifndef NO_ARG_VALIDATION
int INTERNAL  PtlInternalCTHandleValidator(ptl_handle_ct_t handle,
                                           uint_fast8_t    none_ok);
#endif

#endif
