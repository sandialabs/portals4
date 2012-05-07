#ifndef PTL_INTERNAL_MD_H
#define PTL_INTERNAL_MD_H

#include "ptl_visibility.h"
#include "ptl_internal_ints.h"

void INTERNAL PtlInternalMDNISetup(const uint_fast8_t ni,
                                   const ptl_size_t   limit);
void INTERNAL       PtlInternalMDNITeardown(const uint_fast8_t ni_num);
uint8_t INTERNAL *  PtlInternalMDDataPtr(ptl_handle_md_t handle);
ptl_size_t INTERNAL PtlInternalMDLength(ptl_handle_md_t handle);
ptl_md_t INTERNAL * PtlInternalMDFetch(ptl_handle_md_t handle);
void INTERNAL       PtlInternalMDPosted(ptl_handle_md_t handle);
void INTERNAL       PtlInternalMDCleared(ptl_handle_md_t handle);
#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalMDHandleValidator(ptl_handle_md_t handle,
                                          uint_fast8_t    care_about_ct);
#endif
#ifdef USE_TRANSFER_ENGINE
# ifdef REGISTER_ON_BIND
ptl_size_t INTERNAL PtlInternalMDXFEHandle(ptl_handle_md_t handle);
# endif
#endif

#endif /* ifndef PTL_INTERNAL_MD_H */
/* vim:set expandtab: */
