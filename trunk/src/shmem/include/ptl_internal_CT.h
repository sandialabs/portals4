#ifndef PTL_INTERNAL_CT_H
#define PTL_INTERNAL_CT_H

void PtlInternalCTNISetup(
    unsigned int ni,
    ptl_size_t limit);
void PtlInternalCTNITeardown(
    int ni_num);
int PtlInternalCTHandleValidator(
    ptl_handle_ct_t handle,
    int none_ok);

#endif
/* vim:set expandtab */
