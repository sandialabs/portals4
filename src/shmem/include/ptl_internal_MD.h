#ifndef PTL_INTERNAL_MD_H
#define PTL_INTERNAL_MD_H

#include "ptl_visibility.h"

void INTERNAL       PtlInternalMDNISetup(unsigned int ni,
                                         ptl_size_t limit);
void INTERNAL       PtlInternalMDNITeardown(unsigned int ni_num);
char INTERNAL *     PtlInternalMDDataPtr(ptl_handle_md_t handle);
ptl_size_t INTERNAL PtlInternalMDLength(ptl_handle_md_t handle);
ptl_md_t INTERNAL * PtlInternalMDFetch(ptl_handle_md_t handle);
void INTERNAL       PtlInternalMDPosted(ptl_handle_md_t handle);
void INTERNAL       PtlInternalMDCleared(ptl_handle_md_t handle);
#ifndef NO_ARG_VALIDATION
int INTERNAL        PtlInternalMDHandleValidator(ptl_handle_md_t handle,
                                                 int care_about_ct);
#endif

#endif /* ifndef PTL_INTERNAL_MD_H */
/* vim:set expandtab: */
