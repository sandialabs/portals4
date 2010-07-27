#ifndef PTL_INTERNAL_MD_H
#define PTL_INTERNAL_MD_H

void PtlInternalMDNISetup(
    unsigned int ni,
    ptl_size_t limit);
void PtlInternalMDNITeardown(
    unsigned int ni_num);
int PtlInternalMDHandleValidator(
    ptl_handle_md_t handle);

#endif
