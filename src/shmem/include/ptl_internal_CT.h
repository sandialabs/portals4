#ifndef PTL_INTERNAL_CT_H
#define PTL_INTERNAL_CT_H

#include "ptl_visibility.h"

void INTERNAL PtlInternalCTNISetup(unsigned int ni,
                                   ptl_size_t limit);
void INTERNAL PtlInternalCTNITeardown(int ni_num);
int INTERNAL  PtlInternalCTHandleValidator(ptl_handle_ct_t handle,
                                           int none_ok);
void INTERNAL PtlInternalCTSuccessInc(ptl_handle_ct_t ct_handle,
                                      ptl_size_t increment);
void INTERNAL PtlInternalCTFailureInc(ptl_handle_ct_t ct_handle);

#endif /* ifndef PTL_INTERNAL_CT_H */
/* vim:set expandtab: */
