#ifndef PTL_INTERNAL_MD_H
#define PTL_INTERNAL_MD_H

#include "portals4.h"
#include <ptl_visibility.h>

struct ptl_internal_md_t {
    char in_use;
};

typedef struct ptl_internal_md_t ptl_internal_md_t;

#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalMDHandleValidator(ptl_handle_md_t handle,
                                          uint_fast8_t    care_about_ct);
#endif

#endif
/* vim:set expandtab: */
