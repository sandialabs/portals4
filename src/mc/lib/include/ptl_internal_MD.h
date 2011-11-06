#ifndef PTL_INTERNAL_MD_H
#define PTL_INTERNAL_MD_H

#include "portals4.h"
#include <ptl_visibility.h>

ptl_size_t INTERNAL PtlInternalMDLength(ptl_handle_md_t handle);
ptl_md_t INTERNAL * PtlInternalMDFetch(ptl_handle_md_t handle);


#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalMDHandleValidator(ptl_handle_md_t handle,
                                          uint_fast8_t    care_about_ct);

#endif


#endif
