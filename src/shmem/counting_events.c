/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <limits.h>		       /* for UINT_MAX */

/* Internals */
#include "ptl_visibility.h"

const ptl_handle_ct_t PTL_CT_NONE = 0x5fffffff; /* (2<<29) & 0x1fffffff */

int API_FUNC PtlCTAlloc(
    ptl_handle_ni_t ni_handle,
    ptl_ct_type_t ct_type,
    ptl_handle_ct_t * ct_handle)
{
    return PTL_FAIL;
}

int API_FUNC PtlCTFree(
    ptl_handle_ct_t ct_handle)
{
    return PTL_FAIL;
}

int API_FUNC PtlCTGet(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t * event)
{
    return PTL_FAIL;
}

int API_FUNC PtlCTWait(
    ptl_handle_ct_t ct_handle,
    ptl_size_t test)
{
    return PTL_FAIL;
}

int API_FUNC PtlCTSet(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t test)
{
    return PTL_FAIL;
}

int API_FUNC PtlCTInc(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t increment)
{
    return PTL_FAIL;
}
