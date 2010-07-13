/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>

int PtlMDBind(
    ptl_handle_ni_t ni_handle,
    ptl_md_t * md,
    ptl_handle_md_t * md_handle)
{
    return PTL_FAIL;
}

int PtlMDRelease(
    ptl_handle_md_t md_handle)
{
    return PTL_FAIL;
}
