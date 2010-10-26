#ifndef PTL_INTERNAL_MD_H
#define PTL_INTERNAL_MD_H

void PtlInternalMDNISetup(
    unsigned int ni,
    ptl_size_t limit);
void PtlInternalMDNITeardown(
    unsigned int ni_num);
int PtlInternalMDHandleValidator(
    ptl_handle_md_t handle,
    int care_about_ct);

char *PtlInternalMDDataPtr(
    ptl_handle_md_t handle);
ptl_size_t PtlInternalMDLength(
    ptl_handle_md_t handle);
ptl_md_t *PtlInternalMDFetch(
    ptl_handle_md_t handle);
void PtlInternalMDPosted(
    ptl_handle_md_t handle);
void PtlInternalMDCleared(
    ptl_handle_md_t handle);

#endif
/* vim:set expandtab */
