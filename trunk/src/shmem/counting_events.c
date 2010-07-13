/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <limits.h>		       /* for UINT_MAX */

const ptl_handle_ct_t PTL_CT_NONE = UINT_MAX;

int PtlCTAlloc(
    ptl_handle_ni_t ni_handle,
    ptl_ct_type_t ct_type,
    ptl_handle_ct_t * ct_handle)
{
    return PTL_FAIL;
}

int PtlCTFree(
    ptl_handle_ct_t ct_handle)
{
    return PTL_FAIL;
}

int PtlCTGet(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t * event)
{
    return PTL_FAIL;
}

int PtlCTWait(
    ptl_handle_ct_t ct_handle,
    ptl_size_t test)
{
    return PTL_FAIL;
}

int PtlCTSet(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t test)
{
    return PTL_FAIL;
}

int PtlCTInc(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t increment)
{
    return PTL_FAIL;
}
